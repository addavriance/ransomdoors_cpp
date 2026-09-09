#include "Sequencer.hpp"

namespace rd {

void Sequencer::Start() {
    EnterPhase(PhaseId::Warning);
}

void Sequencer::Reset() {
    EnterPhase(PhaseId::Idle);
}

void Sequencer::Update(std::uint32_t deltaMs) {
    const PhaseSpec& phase = FindPhase(currentId_);
    if (phase.durationMs == 0) return; // signal-only phase

    elapsedMs_ += deltaMs;
    if (elapsedMs_ >= phase.durationMs) {
        EnterPhase(phase.onTimeout);
    }
}

void Sequencer::ReportSignal() {
    const PhaseSpec& phase = FindPhase(currentId_);
    if (phase.exitRule != ExitRule::ExternalSignal) return;
    if (currentId_ == PhaseId::TimedOut) return; // see ReportHardModeDecision
    EnterPhase(phase.onSignal);
}

// only entry point for RealHardMode
void Sequencer::ReportHardModeDecision(bool hardModeEnabled) {
    if (currentId_ != PhaseId::TimedOut) return;
    EnterPhase(hardModeEnabled ? PhaseId::RealHardMode : PhaseId::FakeCrash);
}

void Sequencer::EnterPhase(PhaseId id) {
    currentId_ = id;
    elapsedMs_ = 0;
    if (onPhaseEnter_) onPhaseEnter_(FindPhase(id));
}

} // namespace rd
