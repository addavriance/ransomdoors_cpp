#pragma once

#include <cstdint>
#include <string_view>

namespace rd {

enum class PhaseId {
    Idle,
    Warning,
    DownloadJumpscare,
    RansomActive,
    Resolved,
    TimedOut,
    FakeCrash,
    RealHardMode,
};

enum class ExitRule {
    Timeout,         // auto-advance at durationMs
    ExternalSignal,  // advance when condition met
    Terminal,        // resets only via Reset()
};

struct PhaseSpec {
    PhaseId id;
    std::string_view name;
    std::uint32_t durationMs;
    ExitRule exitRule;
    PhaseId onTimeout;
    PhaseId onSignal;
    std::string_view music;
    std::string_view sfx;
};

// state machine as data; not constexpr since Config patches RansomActive's durationMs at startup (see SetPhaseDurationMs)
inline PhaseSpec kSequence[] = {
    { PhaseId::Idle, "Idle", 0, ExitRule::ExternalSignal, PhaseId::Idle, PhaseId::Warning, "", "" },

    { PhaseId::Warning, "Warning", 1100, ExitRule::ExternalSignal,
      PhaseId::Idle, PhaseId::DownloadJumpscare, "", "spawn" },

    { PhaseId::DownloadJumpscare, "DownloadJumpscare", 2000, ExitRule::Timeout,
      PhaseId::RansomActive, PhaseId::RansomActive, "", "attack" },

    { PhaseId::RansomActive, "RansomActive", 78000, ExitRule::ExternalSignal,
      PhaseId::TimedOut, PhaseId::Resolved, "layer1", "" },

    // sfx empty: ThankYouWindow's onReveal fires "thankyou" itself instead
    { PhaseId::Resolved, "Resolved", 3000, ExitRule::Timeout,
      PhaseId::Idle, PhaseId::Idle, "", "" },

    { PhaseId::TimedOut, "TimedOut", 0, ExitRule::ExternalSignal,
      PhaseId::TimedOut, PhaseId::FakeCrash, "", "" },

    { PhaseId::FakeCrash, "FakeCrash", 2500, ExitRule::Timeout,
      PhaseId::Idle, PhaseId::Idle, "", "attack" },

    { PhaseId::RealHardMode, "RealHardMode", 1000, ExitRule::Timeout,
      PhaseId::Idle, PhaseId::Idle, "", "attack" },
};

inline PhaseSpec& FindPhase(PhaseId id) {
    for (auto& phase : kSequence) {
        if (phase.id == id) return phase;
    }
    return kSequence[0];
}

inline void SetPhaseDurationMs(PhaseId id, std::uint32_t durationMs) {
    FindPhase(id).durationMs = durationMs;
}

} // namespace rd
