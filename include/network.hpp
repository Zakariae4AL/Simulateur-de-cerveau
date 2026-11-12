#pragma once

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

// =============================================================================
//  A small piece of cortex: 1000 Izhikevich neurons (Izhikevich, 2003)
//
//  - 80% excitatory neurons (they push other neurons to fire)
//  - 20% inhibitory neurons (they stop other neurons from firing)
//  - every neuron is connected to every other one with a random strength
//  - random "thalamic" input arrives all the time, like input from the senses
//
//  Nothing in the code says "make waves", yet the neurons synchronize by
//  themselves and produce brain rhythms (alpha and gamma waves), just like
//  what an EEG measures on a real head.
// =============================================================================

namespace brain {

inline constexpr double kPi = 3.14159265358979323846;

struct Spike {
    int time_ms;
    int neuron;
};

class CorticalNetwork {
public:
    CorticalNetwork(int excitatory = 800, int inhibitory = 200, unsigned seed = 42)
        : Ne_(excitatory), Ni_(inhibitory), N_(excitatory + inhibitory),
          a_(N_), b_(N_), c_(N_), d_(N_), v_(N_), u_(N_),
          S_(static_cast<size_t>(N_) * N_), I_(N_), rng_(seed) {
        std::uniform_real_distribution<double> U(0.0, 1.0);

        // Excitatory: mostly regular spiking, a few bursting/chattering cells.
        for (int i = 0; i < Ne_; ++i) {
            double r = U(rng_);
            a_[i] = 0.02;
            b_[i] = 0.2;
            c_[i] = -65.0 + 15.0 * r * r;
            d_[i] = 8.0 - 6.0 * r * r;
        }
        // Inhibitory: fast spiking to low-threshold spiking interneurons.
        for (int i = Ne_; i < N_; ++i) {
            double r = U(rng_);
            a_[i] = 0.02 + 0.08 * r;
            b_[i] = 0.25 - 0.05 * r;
            c_[i] = -65.0;
            d_[i] = 2.0;
        }
        for (int i = 0; i < N_; ++i) {
            v_[i] = -65.0;
            u_[i] = b_[i] * v_[i];
        }
        // Synapses, stored by presynaptic neuron so that one spike's
        // fan-out to all its targets is a contiguous (cache friendly) row.
        for (int pre = 0; pre < N_; ++pre)
            for (int post = 0; post < N_; ++post)
                S_[static_cast<size_t>(pre) * N_ + post] =
                    pre < Ne_ ? 0.5 * U(rng_) : -U(rng_);
    }

    // Simulate `ms` milliseconds and return every spike that happened.
    std::vector<Spike> run(int ms) {
        std::vector<Spike> spikes;
        std::normal_distribution<double> noise(0.0, 1.0);
        std::vector<int> fired;
        fired.reserve(N_);

        for (int step = 0; step < ms; ++step, ++t_) {
            // 1) random sensory (thalamic) input
            for (int i = 0; i < N_; ++i)
                I_[i] = (i < Ne_ ? 5.0 : 2.0) * noise(rng_);

            // 2) who fires now?
            fired.clear();
            for (int i = 0; i < N_; ++i) {
                if (v_[i] >= 30.0) {
                    fired.push_back(i);
                    spikes.push_back({t_, i});
                    v_[i] = c_[i];
                    u_[i] += d_[i];
                }
            }

            // 3) spikes travel through synapses to all other neurons
            for (int pre : fired) {
                const double* row = &S_[static_cast<size_t>(pre) * N_];
                for (int post = 0; post < N_; ++post) I_[post] += row[post];
            }

            // 4) update every neuron's voltage
            for (int i = 0; i < N_; ++i) {
                v_[i] += 0.5 * (0.04 * v_[i] * v_[i] + 5.0 * v_[i] + 140.0 - u_[i] + I_[i]);
                v_[i] += 0.5 * (0.04 * v_[i] * v_[i] + 5.0 * v_[i] + 140.0 - u_[i] + I_[i]);
                v_[i] = std::min(v_[i], 30.0);  // keep numbers sane on a spike
                u_[i] += a_[i] * (b_[i] * v_[i] - u_[i]);
            }
        }
        return spikes;
    }

    int excitatory() const { return Ne_; }
    int inhibitory() const { return Ni_; }
    int size() const { return N_; }

private:
    int Ne_, Ni_, N_;
    std::vector<double> a_, b_, c_, d_, v_, u_;
    std::vector<double> S_;  // synaptic weights [pre][post]
    std::vector<double> I_;  // input current this ms
    std::mt19937 rng_;
    int t_ = 0;
};

// Power spectrum of the population activity (spikes per ms), like an EEG.
// Returns power for every whole frequency from 0 Hz to maxHz (index = Hz).
inline std::vector<double> powerSpectrum(const std::vector<Spike>& spikes, int ms, int maxHz = 100) {
    std::vector<double> x(ms, 0.0);
    for (const auto& s : spikes)
        if (s.time_ms >= 0 && s.time_ms < ms) x[s.time_ms] += 1.0;

    double mean = 0.0;
    for (double v : x) mean += v;
    mean /= ms;
    for (int t = 0; t < ms; ++t) {
        // Hann window reduces spectral leakage at the edges of the recording.
        double w = 0.5 - 0.5 * std::cos(2.0 * kPi * t / (ms - 1));
        x[t] = (x[t] - mean) * w;
    }

    std::vector<double> power(maxHz + 1, 0.0);
    for (int f = 1; f <= maxHz; ++f) {  // plain DFT: tiny signal, no FFT needed
        double re = 0.0, im = 0.0;
        for (int t = 0; t < ms; ++t) {
            double phase = 2.0 * kPi * f * t / 1000.0;
            re += x[t] * std::cos(phase);
            im -= x[t] * std::sin(phase);
        }
        power[f] = (re * re + im * im) / ms;
    }
    return power;
}

struct Band {
    const char* name;
    int lowHz, highHz;  // [low, high)
    const char* meaning;
};

inline constexpr Band kEegBands[] = {
    {"theta", 4, 8, "memory, drowsiness"},
    {"alpha", 8, 13, "relaxed, awake"},
    {"beta", 13, 30, "active thinking"},
    {"gamma", 30, 80, "attention, binding"},
};

// Mean power per Hz inside a band.
inline double bandPower(const std::vector<double>& power, const Band& b) {
    double sum = 0.0;
    for (int f = b.lowHz; f < b.highHz && f < static_cast<int>(power.size()); ++f) sum += power[f];
    return sum / (b.highHz - b.lowHz);
}

// Frequency with the highest power between 4 and 80 Hz.
inline int peakFrequency(const std::vector<double>& power) {
    int best = 4;
    for (int f = 4; f < 80 && f < static_cast<int>(power.size()); ++f)
        if (power[f] > power[best]) best = f;
    return best;
}

} // namespace brain
