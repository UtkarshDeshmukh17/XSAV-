#pragma once
#include <string>
#include <chrono>
#include <cstdint>

namespace xsav {

// Kinds of raw filesystem events the File Monitor can produce.
// Downstream stages (Metadata Collection, SHA-256, Static Analysis, ...)
// only ever need to understand this enum, never OS-specific notify codes.
enum class FileEventType : uint8_t {
    Created = 0,
    Modified,
    Deleted,
    RenamedOld,        // rarely surfaced directly; folded into RenamedNew
    RenamedNew,
    AttributesChanged
};

const wchar_t* ToString(FileEventType type);

// The single unit of data that flows from File Monitor into the rest of
// the detection pipeline (Metadata Collection -> SHA-256 -> Local
// Reputation -> Static Analysis -> YARA -> ML -> Behavioral -> Risk Engine).
// Every later stage should be able to do its job from this struct alone,
// or by using `path` to pull more data (open the file, hash it, etc).
struct FileEvent {
    FileEventType type = FileEventType::Created;
    std::wstring path;          // full path of the file/dir affected
    std::wstring oldPath;       // populated only for RenamedNew events
    bool isDirectory = false;
    std::chrono::system_clock::time_point timestamp =
        std::chrono::system_clock::now();

    // Source identifiers, filled in once the agent is multi-host aware.
    // Left empty by File Monitor; Agent Core / Telemetry Collector can
    // stamp these before the event leaves the host.
    std::wstring agentId;
    std::wstring hostId;
};

} // namespace xsav
