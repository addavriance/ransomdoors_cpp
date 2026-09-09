#pragma once

#include <functional>
#include <cstdint>

#include "Sequence.hpp"

namespace rd {

// drives kSequence, no SDL/render/audio deps
class Sequencer {
public:
    using PhaseCallback = std::function<void(const PhaseSpec&)>;

    void SetOnPhaseEnter(PhaseCallback cb) { onPhaseEnter_ = std::move(cb); }

    void Start();
    void Update(std::uint32_t deltaMs);

    void ReportSignal();
    void ReportHardModeDecision(bool hardModeEnabled);

    void Reset();

    PhaseId Current() const { return currentId_; }
    std::uint32_t ElapsedMs() const { return elapsedMs_; }

private:
    void EnterPhase(PhaseId id);

    PhaseId currentId_ = PhaseId::Idle;
    std::uint32_t elapsedMs_ = 0;
    PhaseCallback onPhaseEnter_;
};

} // namespace rd
