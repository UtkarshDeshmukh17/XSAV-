#include <iostream>
#include <csignal>
#include <atomic>
#include "common/EventQueue.h"
#include "file_monitor/WindowsFileMonitor.h"

using namespace xsav;

namespace {
std::atomic<bool> g_stop{false};
void OnSigInt(int) { g_stop.store(true); }
}

// This binary only demonstrates the File Monitor module in isolation:
//   File Created -> [File Monitor] -> EventQueue -> (this consumer loop)
//
// The consumer loop below is deliberately where the pipeline stops today.
// When Metadata Collection is built, it replaces the body of the loop:
// each popped FileEvent gets handed to something like
//   metadataCollector.Process(ev);
// which then hands off to SHA-256, Local Reputation, Static Analysis, etc.
// Nothing in File Monitor, IEventSink, or EventQueue needs to change for
// that to happen - they were built against the interface, not this demo.
int main() {
    std::signal(SIGINT, OnSigInt);

#ifdef _WIN32
    FileMonitorConfig config;
    config.watchPaths = { L"C:\\Users" };
    config.excludePaths = { L"C:\\Users\\Public\\XSAV\\Quarantine" };
    config.recursive = true;
    config.debounceMs = 200;

    EventQueue<FileEvent> queue;
    QueueEventSink sink(queue);

    WindowsFileMonitor monitor(config);
    monitor.SetEventSink(&sink);

    if (!monitor.Start()) {
        std::wcerr << L"Failed to start File Monitor on configured paths.\n";
        return 1;
    }

    std::wcout << L"XSAV File Monitor running. Watching:";
    for (const auto& p : config.watchPaths) std::wcout << L" " << p;
    std::wcout << L"\nPress Ctrl+C to stop.\n\n";

    while (!g_stop.load()) {
        auto item = queue.Pop();
        if (!item) break; // queue was shut down

        const FileEvent& ev = *item;
        std::wcout << L"[" << ToString(ev.type) << L"] " << ev.path;
        if (!ev.oldPath.empty()) {
            std::wcout << L"  (was: " << ev.oldPath << L")";
        }
        std::wcout << L"\n";

        // ---------------- INTEGRATION POINT ----------------
        // metadataCollector.Process(ev);
        // -----------------------------------------------------
    }

    monitor.Stop();
    queue.Shutdown();
    return 0;
#else
    std::cerr << "This demo uses WindowsFileMonitor, which is Windows-only. "
                 "Linux (inotify) and macOS (FSEvents) implementations of "
                 "IFileMonitor are a later phase per the product roadmap.\n";
    return 1;
#endif
}
