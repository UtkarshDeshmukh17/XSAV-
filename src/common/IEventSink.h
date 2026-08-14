#pragma once
#include "Event.h"

namespace xsav {

// Implemented by whatever consumes raw filesystem events.
// File Monitor knows nothing about who its sink is - it could be
// the real-time detection pipeline, a queue adapter (see EventQueue.h),
// a unit test harness, or later, Process/Network/Registry Monitor
// wiring their own correlated events in alongside file events.
//
// NEXT STAGE (not built yet): Metadata Collection will implement this
// interface (or sit downstream of QueueEventSink) and, for each event,
// pull file metadata (size, timestamps, owner, PE header info, etc.)
// before handing off to SHA-256 hashing.
class IEventSink {
public:
    virtual ~IEventSink() = default;
    virtual void OnFileEvent(const FileEvent& event) = 0;
};

} // namespace xsav
