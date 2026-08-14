# File Monitor Module

Status: **complete for Windows (user-mode)**
Position in pipeline: first stage of the File Detection Engine

```
File Created
    ↓
[ File Monitor ]   <-- this module
    ↓
Metadata Collection   (not built yet)
    ↓
SHA-256
    ↓
Local Reputation Check
    ↓
Static Analysis
    ↓
YARA Rules
    ↓
ML Classification
    ↓
Behavioral Analysis if needed
    ↓
Risk Engine
    ↓
Verdict
```

## What this module does

Watches configured directory roots (recursively) and emits a `FileEvent`
for every create, modify, delete, or rename the OS reports. On Windows
this is implemented with `ReadDirectoryChangesW`, one dedicated OS thread
per watched root, using overlapped I/O so the thread never blocks
indefinitely and can be cleanly stopped.

It intentionally does nothing else: no hashing, no metadata extraction,
no verdicts. That's by design — every later stage in the diagram above
is a separate module and should be able to consume `FileEvent`s without
knowing anything about Windows, `ReadDirectoryChangesW`, or threads.

## Files

| File | Purpose |
|---|---|
| `src/common/Event.h` / `.cpp` | `FileEvent` struct + `FileEventType` enum — the data contract every pipeline stage speaks |
| `src/common/IEventSink.h` | Interface a consumer implements to receive events |
| `src/common/EventQueue.h` | Thread-safe queue + `QueueEventSink` adapter, decouples the OS watch thread from pipeline processing |
| `src/file_monitor/IFileMonitor.h` | Platform-agnostic contract (`Start`/`Stop`/`SetEventSink`) |
| `src/file_monitor/WindowsFileMonitor.h` / `.cpp` | Windows implementation via `ReadDirectoryChangesW` |
| `src/main.cpp` | Standalone demo: starts the monitor, prints events, marks the exact integration point for the next stage |

## Design choices worth knowing

- **Interface-first (`IFileMonitor`)** — Agent Core will hold an
  `IFileMonitor*`, never a concrete `WindowsFileMonitor`. When
  `LinuxFileMonitor` (inotify) and `MacFileMonitor` (FSEvents) are built,
  nothing that consumes file events has to change.
- **Sink-based output (`IEventSink`)** — the monitor doesn't know or
  care what happens to a `FileEvent` after `OnFileEvent` is called. Today
  that's `QueueEventSink`; tomorrow it could be Metadata Collection
  directly, or a fan-out to multiple consumers.
- **Queue hand-off** — the OS watch thread pushes into an
  `EventQueue<FileEvent>` and returns immediately. A separate consumer
  thread pops and does the (potentially slow) pipeline work. This matters
  because `ReadDirectoryChangesW` has a fixed-size kernel buffer — if the
  watch thread stalls, Windows starts dropping notifications
  (`ERROR_NOTIFY_ENUM_DIR`), which this module already detects and
  surfaces as a "buffer overflowed, treat state as unknown" signal.
- **Debouncing** — rapid repeated `MODIFIED` events on the same path
  (common during a large file write) are collapsed within a configurable
  window (`FileMonitorConfig::debounceMs`) so downstream stages aren't
  re-hashing a file 200 times during one write.
- **Rename handling** — `RENAMED_OLD_NAME` + `RENAMED_NEW_NAME` pairs are
  folded into a single `RenamedNew` event carrying both paths, since a
  rename is often how malware stages itself under a trusted-looking name.
- **Exclusions** — `FileMonitorConfig::excludePaths` exists so the
  agent's own quarantine/working directories don't create feedback loops
  (e.g. the Quarantine Manager moving a file in triggers a new scan of
  itself).

## How to wire in the next stage (Metadata Collection)

You have two options, and both work with zero changes to this module:

**Option A — implement `IEventSink` directly**

```cpp
class MetadataCollector : public xsav::IEventSink {
public:
    void OnFileEvent(const xsav::FileEvent& event) override {
        // pull metadata, then hand off to SHA-256 stage
    }
};

MetadataCollector collector;
monitor.SetEventSink(&collector);
```

**Option B — consume off the queue (recommended, matches `main.cpp`)**

Keep the `QueueEventSink` wiring as-is, and replace the body of the
consumer loop in `main.cpp`:

```cpp
while (!g_stop.load()) {
    auto item = queue.Pop();
    if (!item) break;
    metadataCollector.Process(*item);   // <-- next stage starts here
}
```

Option B is preferred once Metadata Collection does real work, because it
keeps the OS notification thread completely isolated from processing
latency.

## Build

Requires a Windows toolchain (MSVC or clang-cl) — `ReadDirectoryChangesW`
is a Win32 API and has no meaning on Linux/macOS. On non-Windows hosts,
`cmake` will configure and the `xsav_common` interfaces will compile, but
`WindowsFileMonitor.cpp` and the demo executable are `#ifdef`'d out.

```bash
cmake -B build -S .
cmake --build build --config Release
build\Release\xsav_file_monitor_demo.exe
```

## Not built yet (by design, today's scope was File Monitor only)

Metadata Collection, SHA-256, Local Reputation, Static Analysis, YARA,
ML Classification, Behavioral Analysis, Risk Engine — plus every other
Agent Core component (Process/Network/Registry/Persistence Monitor,
Quarantine Manager, Response Engine, Telemetry Collector, Update Manager,
Secure Communication Module). Placeholder folders exist under `src/` for
these so the repo layout already matches the full agent architecture;
each is currently empty.
