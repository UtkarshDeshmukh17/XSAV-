#pragma once
#ifdef _WIN32

#include <windows.h>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include "IFileMonitor.h"

namespace xsav {

// Windows implementation of IFileMonitor, built on ReadDirectoryChangesW.
// One OS thread per watched root path. Each thread issues an overlapped
// ReadDirectoryChangesW call, waits on either "data ready" or "stop
// requested", parses the returned FILE_NOTIFY_INFORMATION records into
// FileEvent objects, and pushes them to whatever sink was configured
// (see SetEventSink).
class WindowsFileMonitor : public IFileMonitor {
public:
    explicit WindowsFileMonitor(FileMonitorConfig config);
    ~WindowsFileMonitor() override;

    bool Start() override;
    void Stop() override;
    bool IsRunning() const override { return running_.load(); }
    void SetEventSink(IEventSink* sink) override { sink_ = sink; }

private:
    struct WatchContext {
        HANDLE dirHandle = INVALID_HANDLE_VALUE;
        std::wstring rootPath;
        std::vector<BYTE> buffer;
        OVERLAPPED overlapped{};
        HANDLE stopEvent = nullptr;
    };

    void WatchLoop(WatchContext* ctx);
    void ProcessBuffer(WatchContext* ctx, DWORD bytesTransferred);
    void Emit(FileEventType type, const std::wstring& path, bool isDir,
              const std::wstring& oldPath = L"");
    bool ShouldDebounce(const std::wstring& path);
    bool IsExcluded(const std::wstring& path) const;

    FileMonitorConfig config_;
    IEventSink* sink_ = nullptr;
    std::atomic<bool> running_{false};

    std::vector<std::unique_ptr<WatchContext>> watches_;
    std::vector<std::thread> threads_;

    std::mutex debounceMutex_;
    std::unordered_map<std::wstring, std::chrono::steady_clock::time_point> lastSeen_;
};

} // namespace xsav

#endif // _WIN32
