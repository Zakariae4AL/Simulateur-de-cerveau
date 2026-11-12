#pragma once

// =============================================================================
//  Izhikevich (2003) spiking neuron model
//
//    dv/dt = 0.04 v^2 + 5 v + 140 - u + I     (membrane voltage, mV)
//    du/dt = a (b v - u)                      (recovery: K+ channels etc.)
//    if v >= 30 mV  ->  spike!  v = c,  u = u + d
//
//  Just 4 numbers (a, b, c, d) reproduce the firing patterns of real cortical
//  neurons, which is why this model is used in large brain simulations.
// =============================================================================

namespace brain {

struct NeuronParams {
    double a;   // speed of recovery
    double b;   // sensitivity of recovery to voltage
    double c;   // voltage reset after a spike (mV)
    double d;   // recovery boost after a spike
    const char* name;
    const char* description;
};

namespace types {
inline constexpr NeuronParams RegularSpiking{
    0.02, 0.20, -65.0, 8.0, "Regular Spiking (RS)",
    "Most common excitatory neuron in the cortex. Fires, then slows down (adaptation)."};
inline constexpr NeuronParams IntrinsicBursting{
    0.02, 0.20, -55.0, 4.0, "Intrinsically Bursting (IB)",
    "Starts with a fast burst of spikes, then switches to single spikes."};
inline constexpr NeuronParams Chattering{
    0.02, 0.20, -50.0, 2.0, "Chattering (CH)",
    "Fires repeating high-frequency bursts. Found in the visual cortex."};
inline constexpr NeuronParams FastSpiking{
    0.10, 0.20, -65.0, 2.0, "Fast Spiking (FS)",
    "Inhibitory interneuron: fires very fast without slowing down. The brain's brakes."};
} // namespace types

class Neuron {
public:
    explicit Neuron(const NeuronParams& params = types::RegularSpiking)
        : p_(params), v_(-65.0), u_(params.b * -65.0) {}

    // Advance the neuron by 1 ms with input current I.
    // Uses two 0.5 ms Euler half-steps for v (as in Izhikevich's reference code).
    // Returns true if the neuron fired a spike during this millisecond.
    bool step(double I) {
        v_ += 0.5 * (0.04 * v_ * v_ + 5.0 * v_ + 140.0 - u_ + I);
        v_ += 0.5 * (0.04 * v_ * v_ + 5.0 * v_ + 140.0 - u_ + I);
        u_ += p_.a * (p_.b * v_ - u_);
        if (v_ >= 30.0) {
            v_ = p_.c;
            u_ += p_.d;
            return true;
        }
        return false;
    }

    void reset() { v_ = -65.0; u_ = p_.b * v_; }

    double voltage() const { return v_; }
    const NeuronParams& params() const { return p_; }

private:
    NeuronParams p_;
    double v_;
    double u_;
};

} // namespace brain
