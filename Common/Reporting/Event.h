#pragma once
#include <cstdint>
#include <string>

namespace hac { namespace reporting {

enum class Severity : uint8_t {
    Info     = 1,
    Warn     = 2,
    Critical = 3,
};

enum class DetectionKind : uint8_t {
    Unknown            = 0,
    ProcName           = 1,
    WinName            = 2,
    ModuleHash         = 3,
    RpmRate            = 4,
    WpmTarget          = 5,
    HookIntegrity      = 6,
    IatHook            = 7,
    EatHook            = 8,
    CodeSectionDiverge = 9,
    ThreadInject       = 10,
    KnownBadDriver     = 11,
};

struct AntiCheatEvent {
    std::string   client_id;        // HMAC(hardware_id, install_secret)
    std::string   session_id;       // UUIDv4, per game-launch
    uint64_t      timestamp_utc_ms; // ms since Unix epoch
    std::string   game_id;          // from manifest
    Severity      severity;
    DetectionKind kind;
    std::string   detector_version; // e.g. "proc-name@1.0.0"
    std::string   evidence_json;    // kind-specific JSON object
    std::string   signature;        // HMAC-SHA256 hex over fields above
};

}} // namespace hac::reporting
