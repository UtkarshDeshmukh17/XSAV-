# XSAV Endpoint Agent

Endpoint agent for the XSAV AI-assisted EDR platform. Windows first, then
Linux and macOS. Implemented in C++ (Rust considered later for specific
security-sensitive user-mode components, per product architecture).

## Status

| Component | Status |
|---|---|
| File Monitor | **Done (Windows, user-mode)** — see `docs/FILE_MONITOR.md` |
| Agent Core | Not started |
| Process Monitor | Not started |
| Network Monitor | Not started |
| Registry Monitor | Not started |
| Persistence Monitor | Not started |
| Detection Engine | Not started |
| Behavioral Engine | Not started |
| YARA Engine | Not started |
| ML Engine | Not started |
| Quarantine Manager | Not started |
| Response Engine | Not started |
| Telemetry Collector | Not started |
| Update Manager | Not started |
| Secure Communication Module | Not started |

## Layout

```
xsav-agent/
├── CMakeLists.txt
├── docs/
│   └── FILE_MONITOR.md          # detail on the module built so far
└── src/
    ├── common/                  # shared types: Event, IEventSink, EventQueue
    ├── file_monitor/            # DONE — File Monitor module
    ├── core/                    # placeholder — Agent Core
    ├── process_monitor/         # placeholder
    ├── network_monitor/         # placeholder
    ├── registry_monitor/        # placeholder
    ├── persistence_monitor/     # placeholder
    ├── detection_engine/        # placeholder
    ├── behavioral_engine/       # placeholder
    ├── quarantine_manager/      # placeholder
    ├── response_engine/         # placeholder
    ├── telemetry/               # placeholder
    ├── update_manager/          # placeholder
    ├── secure_comm/             # placeholder
    └── main.cpp                 # standalone demo of File Monitor today
```

Empty component folders exist now so the repo already mirrors the full
Endpoint Agent architecture; they'll be filled in one module at a time,
each following the same pattern used for File Monitor: a small interface
in a shared location, a concrete implementation, and a documented
integration point for the stage that consumes its output.

## Build

```bash
cmake -B build -S .
cmake --build build --config Release
```

Windows toolchain required for the File Monitor demo (it uses
`ReadDirectoryChangesW`, a Win32-only API).
