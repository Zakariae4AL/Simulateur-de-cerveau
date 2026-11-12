// =============================================================================
//  Brain Simulator - LIVE view (SFML 3)
//
//  Opens a window and animates the 1000-neuron cortex from network.hpp:
//    - every dot is one neuron; it flashes when it fires a spike
//    - the bottom trace is the total activity, like an EEG recording
//    - the bars show the brain wave bands, updated live
//
//  Controls:
//    Space        pause / resume
//    Up / Down    simulation speed
//    Left / Right weaker / stronger sensory input
//    S            stimulate 200 neurons (like a sudden touch)
//    R            build a new random brain
//    Esc          quit
// =============================================================================

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "network.hpp"

namespace {

constexpr unsigned kWidth = 1280;
constexpr unsigned kHeight = 720;

// Neuron grid: 40 columns x 25 rows = 1000 neurons.
constexpr int kCols = 40;
constexpr float kCell = 15.f;
constexpr float kGridX = 50.f;
constexpr float kGridY = 100.f;
constexpr float kInhibGap = 10.f;  // small gap before the inhibitory rows

// EEG panel at the bottom.
constexpr float kEegX = 50.f, kEegY = 560.f, kEegW = 1180.f, kEegH = 135.f;
constexpr int kHistoryMs = 1000;  // keep the last second of activity

constexpr std::array<int, 5> kSpeeds = {1, 2, 4, 8, 16};  // simulated ms per frame

// Colors
const sf::Color kBackground(13, 17, 23);
const sf::Color kPanel(22, 27, 34);
const sf::Color kBorder(48, 54, 61);
const sf::Color kText(201, 209, 217);
const sf::Color kDim(139, 148, 158);
const sf::Color kAccent(57, 197, 207);
const sf::Color kExcRest(25, 60, 40), kExcFire(140, 255, 170);
const sf::Color kInhRest(70, 28, 32), kInhFire(255, 120, 115);

sf::Color mix(sf::Color a, sf::Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto lerp = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(x + (y - x) * t);
    };
    return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a)};
}

// Add a filled circle to a vertex array (12 triangles).
void addCircle(sf::VertexArray& va, sf::Vector2f c, float r, sf::Color color) {
    constexpr int kSegments = 12;
    for (int k = 0; k < kSegments; ++k) {
        float a0 = 2.f * static_cast<float>(brain::kPi) * k / kSegments;
        float a1 = 2.f * static_cast<float>(brain::kPi) * (k + 1) / kSegments;
        va.append(sf::Vertex{c, color});
        va.append(sf::Vertex{{c.x + r * std::cos(a0), c.y + r * std::sin(a0)}, color});
        va.append(sf::Vertex{{c.x + r * std::cos(a1), c.y + r * std::sin(a1)}, color});
    }
}

sf::Vector2f neuronPosition(int i, int excitatory) {
    int row = i / kCols, col = i % kCols;
    float gap = i >= excitatory ? kInhibGap : 0.f;
    return {kGridX + col * kCell + kCell / 2.f, kGridY + row * kCell + kCell / 2.f + gap};
}

// Try a few common fonts (Windows, Linux, macOS).
bool loadFont(sf::Font& font) {
    const char* candidates[] = {
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/Library/Fonts/Arial.ttf",
    };
    for (const char* path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && font.openFromFile(path)) return true;
    }
    return false;
}

struct Activity {
    int excitatory = 0;
    int inhibitory = 0;
};

}  // namespace

int main() {
    unsigned seed = 42;
    brain::CorticalNetwork net(800, 200, seed);
    const int N = net.size(), Ne = net.excitatory();

    std::vector<float> glow(N, 0.f);   // 1 = just fired, fades to 0
    std::deque<Activity> history;      // spikes per ms, last second
    std::vector<double> power;         // live spectrum
    int speedIndex = 2;
    bool paused = false;
    int stimulateMsLeft = 0;
    long frame = 0;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 4;
    sf::RenderWindow window(sf::VideoMode({kWidth, kHeight}), "Brain Simulator - live cortex",
                            sf::Style::Titlebar | sf::Style::Close, sf::State::Windowed, settings);
    window.setFramerateLimit(60);

    sf::Font font;
    const bool hasFont = loadFont(font);
    if (!hasFont) std::fprintf(stderr, "No font found: text will not be shown.\n");

    auto drawText = [&](const std::string& s, float x, float y, unsigned size,
                        sf::Color color = kText, bool bold = false) {
        if (!hasFont) return;
        sf::Text text(font, s, size);
        text.setPosition({x, y});
        text.setFillColor(color);
        if (bold) text.setStyle(sf::Text::Bold);
        window.draw(text);
    };

    auto panel = [&](float x, float y, float w, float h) {
        sf::RectangleShape r({w, h});
        r.setPosition({x, y});
        r.setFillColor(kPanel);
        r.setOutlineColor(kBorder);
        r.setOutlineThickness(1.f);
        window.draw(r);
    };

    while (window.isOpen()) {
        // ---------------------------------------------------------------- input
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                switch (key->code) {
                case sf::Keyboard::Key::Escape: window.close(); break;
                case sf::Keyboard::Key::Space: paused = !paused; break;
                case sf::Keyboard::Key::Up:
                    speedIndex = std::min(speedIndex + 1, static_cast<int>(kSpeeds.size()) - 1);
                    break;
                case sf::Keyboard::Key::Down: speedIndex = std::max(speedIndex - 1, 0); break;
                case sf::Keyboard::Key::Right: net.setNoiseScale(std::min(2.0, net.noiseScale() + 0.25)); break;
                case sf::Keyboard::Key::Left: net.setNoiseScale(net.noiseScale() - 0.25); break;
                case sf::Keyboard::Key::S: stimulateMsLeft = 3; break;
                case sf::Keyboard::Key::R: {
                    double noise = net.noiseScale();
                    net = brain::CorticalNetwork(800, 200, ++seed);
                    net.setNoiseScale(noise);
                    history.clear();
                    std::fill(glow.begin(), glow.end(), 0.f);
                    break;
                }
                default: break;
                }
            }
        }

        // ----------------------------------------------------------- simulation
        if (!paused) {
            for (float& g : glow) g *= 0.82f;  // flashes fade out
            for (int k = 0; k < kSpeeds[speedIndex]; ++k) {
                if (stimulateMsLeft > 0) {
                    net.stimulate(0, 200, 20.0);  // the first 10 rows get a strong input
                    --stimulateMsLeft;
                }
                Activity a;
                for (int i : net.step()) {
                    glow[i] = 1.f;
                    (i < Ne ? a.excitatory : a.inhibitory)++;
                }
                history.push_back(a);
                if (static_cast<int>(history.size()) > kHistoryMs) history.pop_front();
            }
            if (frame % 10 == 0 && history.size() >= 250) {
                std::vector<double> x;
                x.reserve(history.size());
                for (const auto& a : history) x.push_back(a.excitatory + a.inhibitory);
                power = brain::powerSpectrum(std::move(x));
            }
        }
        ++frame;

        // -------------------------------------------------------------- drawing
        window.clear(kBackground);

        drawText("BRAIN SIMULATOR", kGridX, 22.f, 26, kAccent, true);
        drawText("1000 Izhikevich neurons, live. Each dot is one neuron: it flashes when it fires.",
                 kGridX, 60.f, 15, kDim);

        // Neurons
        sf::VertexArray dots(sf::PrimitiveType::Triangles);
        sf::VertexArray halos(sf::PrimitiveType::Triangles);
        for (int i = 0; i < N; ++i) {
            bool exc = i < Ne;
            sf::Vector2f p = neuronPosition(i, Ne);
            // Resting color gets a bit brighter as the voltage rises toward threshold.
            float charge = static_cast<float>((net.voltage(i) + 75.0) / 50.0);
            sf::Color rest = mix(kBackground, exc ? kExcRest : kInhRest, 0.6f + 0.4f * charge);
            sf::Color fire = exc ? kExcFire : kInhFire;
            addCircle(dots, p, 4.2f + 1.6f * glow[i], mix(rest, fire, glow[i]));
            if (glow[i] > 0.25f) {
                sf::Color halo = fire;
                halo.a = static_cast<std::uint8_t>(70 * glow[i]);
                addCircle(halos, p, 9.f * glow[i] + 3.f, halo);
            }
        }
        window.draw(halos);
        window.draw(dots);

        // Legend under the grid
        const float legendY = kGridY + 25 * kCell + kInhibGap + 14.f;
        sf::VertexArray legend(sf::PrimitiveType::Triangles);
        addCircle(legend, {kGridX + 6.f, legendY + 9.f}, 5.f, kExcFire);
        addCircle(legend, {kGridX + 340.f, legendY + 9.f}, 5.f, kInhFire);
        window.draw(legend);
        drawText("excitatory (800): push others to fire", kGridX + 18.f, legendY, 13, kDim);
        drawText("inhibitory (200): the brakes", kGridX + 352.f, legendY, 13, kDim);

        // ---------------------------------------------------------- right panel
        const float px = 700.f, pw = 530.f;
        panel(px, kGridY, pw, 150.f);

        int excSpikes = 0, inhSpikes = 0;
        for (const auto& a : history) { excSpikes += a.excitatory; inhSpikes += a.inhibitory; }
        double seconds = std::max<std::size_t>(1, history.size()) / 1000.0;
        // The strongest band is the longest bar in the "brain wave bands" panel.
        const brain::Band* strongest = nullptr;
        double strongestPower = 0.0;
        for (const auto& b : brain::kEegBands) {
            double p = power.empty() ? 0.0 : brain::bandPower(power, b);
            if (p > strongestPower) { strongestPower = p; strongest = &b; }
        }

        char line[128];
        float y = kGridY + 14.f;
        auto stat = [&](const char* label, const std::string& value, sf::Color color = kText) {
            drawText(label, px + 18.f, y, 15, kDim);
            drawText(value, px + 230.f, y, 15, color, true);
            y += 26.f;
        };
        std::snprintf(line, sizeof line, "%.1f s", net.timeMs() / 1000.0);
        stat("Simulated time", line);
        std::snprintf(line, sizeof line, "%d ms / frame%s", kSpeeds[speedIndex], paused ? "  (PAUSED)" : "");
        stat("Speed", line, paused ? kInhFire : kText);
        std::snprintf(line, sizeof line, "%d %%", static_cast<int>(std::lround(net.noiseScale() * 100)));
        stat("Sensory input", line);
        std::snprintf(line, sizeof line, "exc %.1f Hz   inh %.1f Hz",
                      excSpikes / seconds / Ne, inhSpikes / seconds / (N - Ne));
        stat("Firing rate", line);
        if (strongest)
            std::snprintf(line, sizeof line, "%s (%d-%d Hz)", strongest->name, strongest->lowHz,
                          strongest->highHz);
        else std::snprintf(line, sizeof line, "measuring...");
        stat("Strongest band", line, kAccent);

        // Brain wave bands
        const float bandsY = kGridY + 165.f;
        panel(px, bandsY, pw, 150.f);
        drawText("BRAIN WAVE BANDS (last second)", px + 18.f, bandsY + 12.f, 14, kAccent, true);
        double maxBand = 1e-9;
        for (const auto& b : brain::kEegBands)
            if (!power.empty()) maxBand = std::max(maxBand, brain::bandPower(power, b));
        float by = bandsY + 42.f;
        for (const auto& b : brain::kEegBands) {
            double value = power.empty() ? 0.0 : brain::bandPower(power, b) / maxBand;
            std::snprintf(line, sizeof line, "%-5s %2d-%-2d Hz", b.name, b.lowHz, b.highHz);
            drawText(line, px + 18.f, by - 2.f, 14, kText);
            sf::RectangleShape track({200.f, 12.f});
            track.setPosition({px + 150.f, by + 2.f});
            track.setFillColor(kBorder);
            window.draw(track);
            sf::RectangleShape bar({200.f * static_cast<float>(value), 12.f});
            bar.setPosition({px + 150.f, by + 2.f});
            bar.setFillColor(kAccent);
            window.draw(bar);
            drawText(b.meaning, px + 362.f, by - 1.f, 13, kDim);
            by += 25.f;
        }

        // Controls
        const float ctrlY = bandsY + 165.f;
        panel(px, ctrlY, pw, 90.f);
        drawText("CONTROLS", px + 18.f, ctrlY + 12.f, 14, kAccent, true);
        drawText("Space  pause        Up/Down  speed        S  stimulate", px + 18.f, ctrlY + 38.f, 14);
        drawText("Left/Right  sensory input      R  new brain    Esc  quit", px + 18.f, ctrlY + 60.f, 14);

        // ------------------------------------------------------------ EEG trace
        drawText("POPULATION ACTIVITY - what an EEG electrode would roughly see (last second)",
                 kEegX, kEegY - 26.f, 14, kAccent, true);
        panel(kEegX, kEegY, kEegW, kEegH);
        if (history.size() > 1) {
            // Auto-scale, but cap it so one huge burst (key S) doesn't flatten
            // the normal activity: the burst simply goes off the top.
            float maxCount = 25.f;
            for (const auto& a : history)
                maxCount = std::max(maxCount, static_cast<float>(a.excitatory + a.inhibitory));
            maxCount = std::min(maxCount, 70.f);
            sf::VertexArray fill(sf::PrimitiveType::TriangleStrip);
            sf::VertexArray trace(sf::PrimitiveType::LineStrip);
            sf::Color fillColor = kAccent;
            fillColor.a = 45;
            const float bottom = kEegY + kEegH - 6.f;
            for (std::size_t k = 0; k < history.size(); ++k) {
                // Newest sample on the right; the trace scrolls to the left.
                float x = kEegX + kEegW - (history.size() - 1 - k) * (kEegW / (kHistoryMs - 1));
                float count = static_cast<float>(history[k].excitatory + history[k].inhibitory);
                float yv = bottom - std::min(count / maxCount, 1.f) * (kEegH - 14.f);
                fill.append(sf::Vertex{{x, bottom}, fillColor});
                fill.append(sf::Vertex{{x, yv}, fillColor});
                trace.append(sf::Vertex{{x, yv}, kAccent});
            }
            window.draw(fill);
            window.draw(trace);
        }

        window.display();
    }
    return 0;
}
