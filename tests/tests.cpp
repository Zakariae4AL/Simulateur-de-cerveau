// =============================================================================
//  Unit tests for the brain simulator.
//
//  A tiny test runner with no external library: each TEST is a function, and
//  CHECK(condition) records a failure if the condition is false.
//
//  Run:   ctest --test-dir build --output-on-failure
//   or:   ./build/brain-tests
// =============================================================================

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "learning.hpp"
#include "network.hpp"
#include "neuron.hpp"

// ----------------------------------------------------------------- mini runner
namespace {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

int g_failures = 0;

struct Register {
    Register(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

}  // namespace

#define TEST(name)                                       \
    static void name();                                  \
    static Register register_##name(#name, name);        \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("    FAILED  %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

using namespace brain;

// Count spikes of a single neuron with a constant input current.
static int spikesWithCurrent(const NeuronParams& type, double current, int ms) {
    Neuron n(type);
    int spikes = 0;
    for (int t = 0; t < ms; ++t) spikes += n.step(current);
    return spikes;
}

// ======================================================================= neuron

TEST(neuron_starts_at_resting_voltage) {
    Neuron n;
    CHECK(n.voltage() == -65.0);
}

TEST(neuron_without_input_never_fires) {
    CHECK(spikesWithCurrent(types::RegularSpiking, 0.0, 1000) == 0);
    CHECK(spikesWithCurrent(types::FastSpiking, 0.0, 1000) == 0);
}

TEST(neuron_with_strong_input_fires) {
    CHECK(spikesWithCurrent(types::RegularSpiking, 10.0, 1000) > 0);
}

TEST(more_input_means_more_spikes) {
    int weak = spikesWithCurrent(types::RegularSpiking, 5.0, 1000);
    int strong = spikesWithCurrent(types::RegularSpiking, 15.0, 1000);
    CHECK(strong > weak);
}

TEST(fast_spiking_fires_faster_than_regular_spiking) {
    // Fast-spiking interneurons recover quicker (bigger a, smaller d).
    CHECK(spikesWithCurrent(types::FastSpiking, 10.0, 1000) >
          spikesWithCurrent(types::RegularSpiking, 10.0, 1000));
}

TEST(voltage_stays_in_biological_range) {
    Neuron n;
    for (int t = 0; t < 1000; ++t) {
        n.step(10.0);
        CHECK(n.voltage() >= -90.0 && n.voltage() < 30.0);
    }
}

TEST(reset_returns_to_rest) {
    Neuron n;
    for (int t = 0; t < 100; ++t) n.step(10.0);
    n.reset();
    CHECK(n.voltage() == -65.0);
}

// ====================================================================== network

TEST(network_has_800_excitatory_and_200_inhibitory) {
    CorticalNetwork net;
    CHECK(net.size() == 1000);
    CHECK(net.excitatory() == 800);
    CHECK(net.inhibitory() == 200);
}

TEST(same_seed_gives_same_brain) {
    CorticalNetwork a(800, 200, 123), b(800, 200, 123);
    CHECK(a.run(500).size() == b.run(500).size());
}

TEST(firing_rates_are_realistic) {
    // Cortical neurons fire a few spikes per second, not hundreds.
    CorticalNetwork net;
    net.run(300);  // skip the start-up burst
    double rate = net.run(1000).size() / 1000.0;  // spikes/s/neuron
    CHECK(rate > 2.0 && rate < 30.0);
}

TEST(no_sensory_input_means_silence) {
    CorticalNetwork net;
    net.setNoiseScale(0.0);
    CHECK(net.run(500).empty());
}

TEST(stimulation_triggers_a_burst) {
    CorticalNetwork net;
    net.setNoiseScale(0.0);
    net.stimulate(0, 200, 20.0);
    std::size_t spikes = 0;
    for (int k = 0; k < 5; ++k) spikes += net.step().size();
    CHECK(spikes >= 150);
}

TEST(time_advances_one_ms_per_step) {
    CorticalNetwork net;
    net.step();
    net.step();
    CHECK(net.timeMs() == 2);
}

// ===================================================================== spectrum

TEST(spectrum_finds_a_10_hz_rhythm) {
    std::vector<double> x(1000);
    for (int t = 0; t < 1000; ++t) x[t] = 5.0 + 5.0 * std::sin(2.0 * kPi * 10.0 * t / 1000.0);
    std::vector<double> power = powerSpectrum(x);
    CHECK(peakFrequency(power) == 10);
    CHECK(bandPower(power, kEegBands[1]) > bandPower(power, kEegBands[3]));  // alpha > gamma
}

TEST(flat_activity_has_no_power) {
    std::vector<double> power = powerSpectrum(std::vector<double>(1000, 3.0));
    double total = 0.0;
    for (double p : power) total += p;
    CHECK(total < 1e-9);
}

// ===================================================================== learning

TEST(stdp_learns_the_bell_but_not_the_light) {
    ConditioningCircuit c(7);
    double bellBefore = c.meanBellWeight();
    double lightBefore = c.meanLightWeight();
    int bellSpikesBefore = c.trial(true, false, false, false).outputSpikes;

    for (int k = 0; k < 40; ++k) {
        c.trial(true, false, true, true);   // bell + food
        c.trial(false, true, false, true);  // light alone
    }

    CHECK(c.meanBellWeight() > bellBefore * 3.0);       // bell synapses grew a lot
    CHECK(c.meanLightWeight() <= lightBefore + 1e-9);   // light synapses did not grow
    int bellSpikesAfter = c.trial(true, false, false, false).outputSpikes;
    int lightSpikesAfter = c.trial(false, true, false, false).outputSpikes;
    CHECK(bellSpikesBefore == 0);
    CHECK(bellSpikesAfter > 0);                         // the bell alone now works
    CHECK(lightSpikesAfter == 0);
}

TEST(no_learning_when_learning_is_off) {
    ConditioningCircuit c(7);
    double before = c.meanBellWeight();
    for (int k = 0; k < 10; ++k) c.trial(true, false, true, false);
    CHECK(c.meanBellWeight() == before);
}

TEST(weights_stay_within_limits) {
    ConditioningCircuit c(7);
    for (int k = 0; k < 300; ++k) c.trial(true, false, true, true);
    CHECK(c.meanBellWeight() <= c.wMax + 1e-9);
    CHECK(c.meanBellWeight() >= 0.0);
}

// ========================================================================= main

int main() {
    int failedTests = 0;
    for (const auto& test : registry()) {
        int before = g_failures;
        test.body();
        bool ok = g_failures == before;
        failedTests += !ok;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", test.name);
    }
    std::printf("\n%zu tests, %d failed\n", registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
