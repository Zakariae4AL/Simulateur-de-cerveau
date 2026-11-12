// =============================================================================
//  Brain Simulator: how neurons fire, talk, make brain waves and learn.
//
//  Usage:
//      brain              interactive menu
//      brain neurons      experiment 1: single neurons
//      brain cortex       experiment 2: 1000-neuron network & brain waves
//      brain learn        experiment 3: Pavlov's dog with STDP learning
//      brain all          run everything
//  Options:  --no-color
// =============================================================================

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "color.hpp"
#include "learning.hpp"
#include "network.hpp"
#include "neuron.hpp"
#include "plot.hpp"

using namespace brain;
namespace C = brain::color;

// -----------------------------------------------------------------------------
//  Experiment 1: single neurons
// -----------------------------------------------------------------------------
void experimentNeurons() {
    plot::heading("EXPERIMENT 1: How a single neuron fires");
    std::cout
        << "A neuron collects electric current from other neurons. When its voltage\n"
        << "reaches about +30 mV it fires a SPIKE (action potential) and resets.\n"
        << "We inject a constant current starting at 50 ms and watch the voltage.\n"
        << "Different neuron types answer the same input in different ways.\n";

    const NeuronParams kinds[] = {types::RegularSpiking, types::IntrinsicBursting,
                                  types::Chattering, types::FastSpiking};
    const int durationMs = 300, stimStart = 50;
    const double current = 10.0;

    for (const auto& kind : kinds) {
        Neuron n(kind);
        std::vector<double> v;
        int spikes = 0;
        for (int t = 0; t < durationMs; ++t) {
            bool fired = n.step(t >= stimStart ? current : 0.0);
            spikes += fired;
            v.push_back(fired ? 30.0 : n.voltage());
        }
        std::cout << '\n' << C::bold() << kind.name << C::reset() << "  -  " << kind.description << '\n';
        plot::trace(v, -80.0, 30.0, 12, 100);
        std::cout << std::string(11, ' ') << C::dim()
                  << std::string(stimStart * 100 / durationMs, '_')
                  << std::string(100 - stimStart * 100 / durationMs, '=')
                  << C::reset() << "  <- input current\n";
        std::cout << "  " << spikes << " spikes in " << durationMs - stimStart
                  << " ms (" << std::fixed << std::setprecision(0)
                  << spikes * 1000.0 / (durationMs - stimStart) << " Hz)\n";
    }
}

// -----------------------------------------------------------------------------
//  Experiment 2: a piece of cortex
// -----------------------------------------------------------------------------
void experimentCortex() {
    plot::heading("EXPERIMENT 2: 1000 neurons talking -> brain waves");
    std::cout
        << "800 excitatory + 200 inhibitory neurons, all connected, with random\n"
        << "sensory noise. Each row below is a group of 25 neurons, each column is\n"
        << "10 ms. Darker characters = more spikes. Watch the vertical stripes:\n"
        << "neurons start firing TOGETHER in rhythms, nobody programmed that.\n\n";

    const int ms = 1000, groupSize = 25, binMs = 10;
    CorticalNetwork net(800, 200, 42);

    auto start = std::chrono::steady_clock::now();
    net.run(300);  // warm-up: let the start-up burst die out before recording
    std::vector<Spike> spikes = net.run(ms);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    for (auto& s : spikes) s.time_ms -= 300;  // recording starts at 0 ms

    const int rows = net.size() / groupSize, cols = ms / binMs;
    std::vector<std::vector<int>> grid(rows, std::vector<int>(cols, 0));
    std::vector<int> population(cols, 0);
    int excSpikes = 0, inhSpikes = 0;
    for (const auto& s : spikes) {
        grid[s.neuron / groupSize][s.time_ms / binMs]++;
        population[s.time_ms / binMs]++;
        (s.neuron < net.excitatory() ? excSpikes : inhSpikes)++;
    }

    const std::string shades = " .:-=+*#%@";
    const int excRows = net.excitatory() / groupSize;
    for (int r = 0; r < rows; ++r) {
        bool exc = r < excRows;
        std::string label = r == 0 ? "excit." : (r == excRows ? "inhib." : "");
        std::cout << std::setw(8) << label << " |" << (exc ? C::green() : C::red());
        for (int c = 0; c < cols; ++c)
            std::cout << shades[std::min<int>(grid[r][c], static_cast<int>(shades.size()) - 1)];
        std::cout << C::reset() << '\n';
    }
    std::cout << std::string(9, ' ') << '+' << std::string(cols, '-') << '\n'
              << std::string(10, ' ') << "0 ms" << std::string(cols - 11, ' ') << "1000 ms\n\n";

    std::cout << C::bold() << "Total activity (what an EEG electrode would roughly see):"
              << C::reset() << '\n';
    plot::histogram(population, 8);
    std::cout << std::string(9, ' ') << '+' << std::string(cols, '-') << "\n\n";

    // Frequency analysis, exactly what neuroscientists do with EEG recordings.
    std::vector<double> power = powerSpectrum(spikes, ms);
    int peakHz = peakFrequency(power);
    const char* peakBand = "gamma";
    double maxBand = 0.0;
    for (const auto& b : kEegBands) {
        maxBand = std::max(maxBand, bandPower(power, b));
        if (peakHz >= b.lowHz && peakHz < b.highHz) peakBand = b.name;
    }

    std::cout << C::bold() << "Brain wave bands (power spectrum of the activity):" << C::reset() << '\n';
    for (const auto& b : kEegBands) {
        std::cout << "  " << std::left << std::setw(6) << b.name << std::right << std::setw(3)
                  << b.lowHz << '-' << std::left << std::setw(3) << b.highHz << std::right << "Hz  "
                  << C::cyan() << plot::bar(bandPower(power, b), maxBand, 30) << C::reset()
                  << "  " << C::dim() << b.meaning << C::reset() << '\n';
    }

    std::cout << '\n' << std::fixed << std::setprecision(1)
              << "  Excitatory firing rate : " << excSpikes * 1000.0 / ms / net.excitatory() << " Hz\n"
              << "  Inhibitory firing rate : " << inhSpikes * 1000.0 / ms / net.inhibitory() << " Hz\n"
              << "  Strongest rhythm       : " << C::bold() << peakHz << " Hz (" << peakBand << ")"
              << C::reset() << '\n'
              << "  Simulated " << spikes.size() << " spikes in " << std::setprecision(2) << secs << " s\n\n"
              << C::dim()
              << "  Nobody programmed these waves. Excitatory cells push activity up,\n"
              << "  inhibitory cells shut it down, and that push-pull creates a rhythm.\n"
              << C::reset();
}

// -----------------------------------------------------------------------------
//  Experiment 3: learning
// -----------------------------------------------------------------------------
void printTrial(const std::string& title, const TrialRecord& rec) {
    const int binMs = 5;
    const int cols = ConditioningCircuit::kTrialMs / binMs;
    auto row = [&](const std::string& label, const char* col, auto&& spikeAt, char mark) {
        std::cout << std::setw(16) << label << " |" << col;
        for (int c = 0; c < cols; ++c) {
            bool any = false;
            for (int t = c * binMs; t < (c + 1) * binMs; ++t) any = any || spikeAt(t);
            std::cout << (any ? mark : ' ');
        }
        std::cout << C::reset() << '\n';
    };

    std::cout << C::bold() << title << C::reset() << '\n';
    for (int i = 0; i < 4; ++i)
        row("bell neuron " + std::to_string(i + 1), C::blue(),
            [&](int t) { return rec.input[i][t]; }, '|');
    for (int i = 0; i < 4; ++i) {
        int j = ConditioningCircuit::kBell + i;
        row("light neuron " + std::to_string(i + 1), C::magenta(),
            [&](int t) { return rec.input[j][t]; }, '|');
    }
    row("food", C::green(), [&](int t) { return rec.food[t]; }, '=');
    row("SALIVATION", C::yellow(), [&](int t) { return rec.output[t]; }, '|');
    std::cout << std::string(17, ' ') << '+' << std::string(cols, '-') << '\n'
              << std::string(18, ' ') << "0 ms" << std::string(cols - 10, ' ') << "250 ms\n"
              << "  -> salivation neuron fired " << C::bold() << rec.outputSpikes
              << " spikes" << C::reset() << "\n\n";
}

void experimentLearning() {
    plot::heading("EXPERIMENT 3: Learning - Pavlov's dog inside a neuron");
    std::cout
        << "A 'salivation' neuron gets input from 20 BELL neurons, 20 LIGHT neurons\n"
        << "and from FOOD. At first only food makes it fire. During training, the\n"
        << "bell is always followed by food, the light is never followed by food.\n"
        << "Synapses change with STDP: input-before-output = stronger.\n\n";

    ConditioningCircuit brainCircuit(7);

    std::cout << C::bold() << C::cyan() << "--- BEFORE training ---" << C::reset() << "\n\n";
    printTrial("Ring the bell (no food):", brainCircuit.trial(true, false, false, false));
    printTrial("Flash the light (no food):", brainCircuit.trial(false, true, false, false));
    printTrial("Give food:", brainCircuit.trial(false, false, true, false));

    std::cout << C::bold() << C::cyan() << "--- TRAINING (bell + food, light alone) ---"
              << C::reset() << "\n\n"
              << "  trial   bell synapses              light synapses\n";
    const int trainingTrials = 40;
    const double wMax = brainCircuit.wMax;
    for (int k = 1; k <= trainingTrials; ++k) {
        brainCircuit.trial(true, false, true, true);   // bell paired with food
        brainCircuit.trial(false, true, false, true);  // light, nothing follows
        if (k == 1 || k % 5 == 0) {
            std::cout << std::setw(7) << k << "   " << C::blue()
                      << plot::bar(brainCircuit.meanBellWeight(), wMax, 20) << C::reset()
                      << std::fixed << std::setprecision(2) << ' ' << brainCircuit.meanBellWeight()
                      << "   " << C::magenta() << plot::bar(brainCircuit.meanLightWeight(), wMax, 20)
                      << C::reset() << ' ' << brainCircuit.meanLightWeight() << '\n';
        }
    }

    std::cout << '\n' << C::bold() << C::cyan() << "--- AFTER training ---" << C::reset() << "\n\n";
    TrialRecord bell = brainCircuit.trial(true, false, false, false);
    TrialRecord light = brainCircuit.trial(false, true, false, false);
    printTrial("Ring the bell (no food):", bell);
    printTrial("Flash the light (no food):", light);

    std::cout << C::bold()
              << (bell.outputSpikes > light.outputSpikes
                      ? "  The bell alone now triggers salivation: the circuit LEARNED.\n"
                      : "  No learning this time. Try more training trials or a bigger aPlus.\n")
              << C::reset() << C::dim()
              << "  Nothing was told what the bell 'means'. Bell spikes kept arriving just\n"
              << "  before the neuron fired, so those synapses grew. That's Hebbian learning.\n"
              << C::reset();
}

// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::string command;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-color") C::enabled = false;
        else command = arg;
    }

    auto runCommand = [](const std::string& cmd) {
        if (cmd == "neurons" || cmd == "1") experimentNeurons();
        else if (cmd == "cortex" || cmd == "2") experimentCortex();
        else if (cmd == "learn" || cmd == "3") experimentLearning();
        else if (cmd == "all" || cmd == "4") { experimentNeurons(); experimentCortex(); experimentLearning(); }
        else return false;
        return true;
    };

    if (!command.empty()) {
        if (!runCommand(command)) {
            std::cerr << "Unknown command '" << command << "'. Use: neurons | cortex | learn | all\n";
            return 1;
        }
        return 0;
    }

    while (true) {
        plot::heading("BRAIN SIMULATOR");
        std::cout << "  1) How a single neuron fires\n"
                  << "  2) 1000 neurons -> brain waves\n"
                  << "  3) Learning: Pavlov's dog with STDP\n"
                  << "  4) Run everything\n"
                  << "  q) Quit\n\n> ";
        std::string choice;
        if (!std::getline(std::cin, choice) || choice == "q" || choice == "Q") break;
        if (!runCommand(choice)) { std::cout << "Please type 1, 2, 3, 4 or q.\n"; continue; }
        std::cout << "\nPress Enter to go back to the menu...";
        std::getline(std::cin, choice);
    }
    return 0;
}
