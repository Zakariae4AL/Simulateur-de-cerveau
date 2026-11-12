#pragma once

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "neuron.hpp"

// =============================================================================
//  Pavlov's dog, inside a neuron
//
//  Circuit:
//      20 "bell"  neurons  ──(weak synapses)──┐
//      20 "light" neurons  ──(weak synapses)──┼──>  SALIVATION neuron
//      "food" (taste)      ──(strong input)───┘
//
//  At the start, only food makes the salivation neuron fire.
//  During training the bell is always followed by food; the light is not.
//
//  Learning rule: STDP (spike-timing-dependent plasticity), measured in real
//  neurons by Bi & Poo (1998):
//    - input spike shortly BEFORE output spike  -> synapse gets stronger
//    - input spike shortly AFTER  output spike  -> synapse gets weaker
//  "Neurons that fire together, wire together."
//
//  After training, the bell ALONE makes the neuron fire. The light still doesn't.
// =============================================================================

namespace brain {

struct TrialRecord {
    int outputSpikes = 0;
    std::vector<std::vector<bool>> input;  // [input neuron][ms]
    std::vector<bool> output;              // [ms]
    std::vector<bool> food;                // [ms]
};

class ConditioningCircuit {
public:
    static constexpr int kBell = 20;
    static constexpr int kLight = 20;
    static constexpr int kInputs = kBell + kLight;
    static constexpr int kTrialMs = 250;   // one trial
    static constexpr int kStimEnd = 150;   // bell/light are on from 0 to 150 ms
    static constexpr int kFoodStart = 30;  // food arrives 30 ms after the bell

    // Model parameters (public so you can experiment with them).
    double inputRateHz = 40.0;  // firing rate of an active input neuron
    double foodCurrent = 10.0;  // drive from the taste of food
    double wMax = 4.0;          // strongest possible synapse
    double aPlus = 0.04;        // potentiation step (pre before post)
    double aMinus = 0.025;      // depression step  (post before pre)
    double tauPlus = 20.0;      // ms, STDP window
    double tauMinus = 20.0;     // ms, STDP window
    double tauSyn = 5.0;        // ms, synaptic current decay

    explicit ConditioningCircuit(unsigned seed = 7)
        : out_(types::RegularSpiking), w_(kInputs), rng_(seed) {
        std::uniform_real_distribution<double> U(0.2, 0.3);
        for (double& w : w_) w = U(rng_);
    }

    // Run one 250 ms trial. If `learn` is true, STDP changes the synapses.
    TrialRecord trial(bool bell, bool light, bool food, bool learn) {
        TrialRecord rec;
        rec.input.assign(kInputs, std::vector<bool>(kTrialMs, false));
        rec.output.assign(kTrialMs, false);
        rec.food.assign(kTrialMs, false);

        out_.reset();
        std::vector<double> preTrace(kInputs, 0.0);  // "this input fired recently"
        double postTrace = 0.0;                      // "the output fired recently"
        double synCurrent = 0.0;

        const double pSpike = inputRateHz / 1000.0;
        const double decayPlus = std::exp(-1.0 / tauPlus);
        const double decayMinus = std::exp(-1.0 / tauMinus);
        const double decaySyn = std::exp(-1.0 / tauSyn);
        std::uniform_real_distribution<double> U(0.0, 1.0);

        for (int t = 0; t < kTrialMs; ++t) {
            const bool stimOn = t < kStimEnd;
            const bool foodOn = food && t >= kFoodStart && t < kStimEnd;
            rec.food[t] = foodOn;

            for (double& x : preTrace) x *= decayPlus;
            postTrace *= decayMinus;
            synCurrent *= decaySyn;

            // Input neurons fire randomly (Poisson) while their stimulus is on.
            for (int j = 0; j < kInputs; ++j) {
                const bool active = stimOn && (j < kBell ? bell : light);
                if (active && U(rng_) < pSpike) {
                    rec.input[j][t] = true;
                    synCurrent += w_[j];
                    preTrace[j] += 1.0;
                    // Input fired AFTER output -> weaken (LTD)
                    if (learn) w_[j] = std::max(0.0, w_[j] - aMinus * postTrace);
                }
            }

            if (out_.step(synCurrent + (foodOn ? foodCurrent : 0.0))) {
                rec.output[t] = true;
                ++rec.outputSpikes;
                postTrace += 1.0;
                // Output fired AFTER inputs -> strengthen them (LTP)
                if (learn)
                    for (int j = 0; j < kInputs; ++j)
                        w_[j] = std::min(wMax, w_[j] + aPlus * preTrace[j]);
            }
        }
        return rec;
    }

    double meanBellWeight() const { return mean(0, kBell); }
    double meanLightWeight() const { return mean(kBell, kInputs); }

private:
    double mean(int from, int to) const {
        double s = 0.0;
        for (int i = from; i < to; ++i) s += w_[i];
        return s / (to - from);
    }

    Neuron out_;
    std::vector<double> w_;  // synaptic weights from inputs to output
    std::mt19937 rng_;
};

} // namespace brain
