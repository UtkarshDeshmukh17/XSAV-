#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../common/IEventSink.h"

namespace xsav {

struct FileMonitorConfig {
    std::vector<std::wstring> watchPaths;      // roots to watch
    std::vector<std::wstring> excludePaths;    // e.g. agent's own quarantine dir
    bool recursive = true;
    uint32_t bufferSizeBytes = 64 * 1024;      // per-directory OS notify buffer
    uint32_t debounceMs = 200;                 // collapse rapid duplicate MODIFY events
};

// Platform-agnostic contract for filesystem watching.
// WindowsFileMonitor (this deliverable) implements it first via
// ReadDirectoryChangesW. LinuxFileMonitor (inotify) and
// MacFileMonitor (FSEvents) implement it later - Agent Core will only
// ever hold an IFileMonitor*, so adding those doesn't touch any code
// that consumes file events.
class IFileMonitor {
public:
    virtual ~IFileMonitor() = default;

    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;

    // The component that receives every FileEvent this monitor produces.
    // Typically a QueueEventSink wrapping an EventQueue<FileEvent>.
    virtual void SetEventSink(IEventSink* sink) = 0;
};

} // namespace xsav
