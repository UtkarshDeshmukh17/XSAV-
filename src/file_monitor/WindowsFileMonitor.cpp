#include "WindowsFileMonitor.h"
#ifdef _WIN32

#include <algorithm>
#include <cwctype>

namespace xsav {

namespace {

std::wstring NormalizePath(const std::wstring& path) {
    std::wstring result = path;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

} // namespace

WindowsFileMonitor::WindowsFileMonitor(FileMonitorConfig config)
    : config_(std::move(config)) {}

WindowsFileMonitor::~WindowsFileMonitor() {
    Stop();
}

bool WindowsFileMonitor::Start() {
    if (running_.load()) return true;
    if (config_.watchPaths.empty()) return false;

    for (const auto& path : config_.watchPaths) {
        auto ctx = std::make_unique<WatchContext>();
        ctx->rootPath = path;
        ctx->buffer.resize(config_.bufferSizeBytes);
        ctx->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        ctx->dirHandle = CreateFileW(
            path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (ctx->dirHandle == INVALID_HANDLE_VALUE) {
            // TODO: route through Telemetry Collector / agent logging
            // once that component exists. For now, skip this root.
            CloseHandle(ctx->stopEvent);
            continue;
        }

        ctx->overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        WatchContext* rawCtx = ctx.get();
        watches_.push_back(std::move(ctx));
        threads_.emplace_back([this, rawCtx] { WatchLoop(rawCtx); });
    }

    running_.store(!watches_.empty());
    return running_.load();
}

void WindowsFileMonitor::Stop() {
    if (!running_.load()) return;
    running_.store(false);

    for (auto& ctx : watches_) {
        if (ctx->stopEvent) SetEvent(ctx->stopEvent);
        if (ctx->dirHandle != INVALID_HANDLE_VALUE) {
            CancelIoEx(ctx->dirHandle, &ctx->overlapped);
        }
    }
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    for (auto& ctx : watches_) {
        if (ctx->dirHandle != INVALID_HANDLE_VALUE) CloseHandle(ctx->dirHandle);
        if (ctx->overlapped.hEvent) CloseHandle(ctx->overlapped.hEvent);
        if (ctx->stopEvent) CloseHandle(ctx->stopEvent);
    }
    threads_.clear();
    watches_.clear();
}

void WindowsFileMonitor::WatchLoop(WatchContext* ctx) {
    const DWORD notifyFilter =
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION |
        FILE_NOTIFY_CHANGE_SECURITY;

    HANDLE waitHandles[2] = { ctx->overlapped.hEvent, ctx->stopEvent };

    while (running_.load()) {
        BOOL ok = ReadDirectoryChangesW(
            ctx->dirHandle,
            ctx->buffer.data(),
            static_cast<DWORD>(ctx->buffer.size()),
            config_.recursive ? TRUE : FALSE,
            notifyFilter,
            nullptr,
            &ctx->overlapped,
            nullptr);

        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            break; // handle likely invalidated (e.g. root removed); drop this watch
        }

        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0 + 1) {
            break; // stop requested
        }

        DWORD transferred = 0;
        if (!GetOverlappedResult(ctx->dirHandle, &ctx->overlapped, &transferred, FALSE)) {
            if (GetLastError() == ERROR_NOTIFY_ENUM_DIR) {
                // Buffer overflowed - a burst of changes was missed.
                // Detection Engine (later stage) should treat this as
                // "unknown state, consider a directory rescan" rather
                // than silently trusting the file inventory it holds.
                continue;
            }
            break;
        }

        ResetEvent(ctx->overlapped.hEvent);
        ProcessBuffer(ctx, transferred);
    }
}

void WindowsFileMonitor::ProcessBuffer(WatchContext* ctx, DWORD bytesTransferred) {
    if (bytesTransferred == 0) return;

    BYTE* base = ctx->buffer.data();
    size_t offset = 0;
    std::wstring pendingOldName; // holds RENAMED_OLD_NAME until RENAMED_NEW_NAME arrives

    while (true) {
        auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(base + offset);

        std::wstring relativeName(info->FileName, info->FileNameLength / sizeof(WCHAR));
        std::wstring fullPath = ctx->rootPath + L"\\" + relativeName;

        if (!IsExcluded(fullPath)) {
            switch (info->Action) {
                case FILE_ACTION_ADDED:
                    Emit(FileEventType::Created, fullPath, false);
                    break;
                case FILE_ACTION_MODIFIED:
                    if (!ShouldDebounce(fullPath)) {
                        Emit(FileEventType::Modified, fullPath, false);
                    }
                    break;
                case FILE_ACTION_REMOVED:
                    Emit(FileEventType::Deleted, fullPath, false);
                    break;
                case FILE_ACTION_RENAMED_OLD_NAME:
                    pendingOldName = fullPath;
                    break;
                case FILE_ACTION_RENAMED_NEW_NAME:
                    // Treated as a Created-equivalent downstream: a rename
                    // can be malware staging itself under a trusted name.
                    Emit(FileEventType::RenamedNew, fullPath, false, pendingOldName);
                    pendingOldName.clear();
                    break;
                default:
                    break;
            }
        }

        if (info->NextEntryOffset == 0) break;
        offset += info->NextEntryOffset;
    }
}

void WindowsFileMonitor::Emit(FileEventType type, const std::wstring& path,
                               bool isDir, const std::wstring& oldPath) {
    if (!sink_) return;
    FileEvent event;
    event.type = type;
    event.path = path;
    event.oldPath = oldPath;
    event.isDirectory = isDir;
    event.timestamp = std::chrono::system_clock::now();
    sink_->OnFileEvent(event);
}

bool WindowsFileMonitor::ShouldDebounce(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(debounceMutex_);
    auto now = std::chrono::steady_clock::now();
    auto key = NormalizePath(path);
    auto it = lastSeen_.find(key);
    if (it != lastSeen_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second).count();
        if (elapsed < config_.debounceMs) {
            it->second = now;
            return true;
        }
    }
    lastSeen_[key] = now;
    return false;
}

bool WindowsFileMonitor::IsExcluded(const std::wstring& path) const {
    std::wstring normalized = NormalizePath(path);
    for (const auto& excl : config_.excludePaths) {
        if (normalized.rfind(NormalizePath(excl), 0) == 0) return true;
    }
    return false;
}

} // namespace xsav

#endif // _WIN32
