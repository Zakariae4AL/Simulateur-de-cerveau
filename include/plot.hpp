#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "color.hpp"

// Small ASCII plotting helpers so the simulation runs in any terminal.
namespace brain::plot {

// Line plot of a time series (1 sample per ms). Each column shows the max of
// its time bin, so 1 ms spikes stay visible after downsampling.
inline void trace(const std::vector<double>& y, double ymin, double ymax,
                  int height = 14, int width = 100) {
    if (y.empty()) return;
    width = std::min<int>(width, static_cast<int>(y.size()));

    std::vector<double> col(width);
    for (int c = 0; c < width; ++c) {
        size_t from = static_cast<size_t>(c) * y.size() / width;
        size_t to = std::max(from + 1, static_cast<size_t>(c + 1) * y.size() / width);
        col[c] = *std::max_element(y.begin() + from, y.begin() + to);
    }

    auto rowOf = [&](double v) {
        double t = std::clamp((v - ymin) / (ymax - ymin), 0.0, 1.0);
        return height - 1 - static_cast<int>(std::lround(t * (height - 1)));
    };

    std::vector<std::string> grid(height, std::string(width, ' '));
    for (int c = 0; c < width; ++c) {
        int r = rowOf(col[c]);
        grid[r][c] = '*';
        if (c > 0) {  // draw a vertical line to the previous point
            int pr = rowOf(col[c - 1]);
            for (int k = std::min(r, pr) + 1; k < std::max(r, pr); ++k) grid[k][c] = '|';
        }
    }

    for (int r = 0; r < height; ++r) {
        double label = ymax - (ymax - ymin) * r / (height - 1);
        std::cout << std::setw(5) << static_cast<int>(std::lround(label)) << " mV |"
                  << color::yellow() << grid[r] << color::reset() << '\n';
    }
    std::cout << std::string(10, ' ') << '+' << std::string(width, '-') << '\n';
    std::string axis = "0 ms";
    std::string end = std::to_string(y.size()) + " ms";
    std::cout << std::string(11, ' ') << axis
              << std::string(std::max<int>(1, width - static_cast<int>(axis.size() + end.size())), ' ')
              << end << '\n';
}

// A horizontal bar like "#########".
inline std::string bar(double value, double maxValue, int width) {
    int n = static_cast<int>(std::lround(std::clamp(value / maxValue, 0.0, 1.0) * width));
    return std::string(n, '#') + std::string(width - n, '.');
}

// Vertical bar chart of counts per bin (used for population activity).
inline void histogram(const std::vector<int>& counts, int height = 8) {
    int maxCount = std::max(1, *std::max_element(counts.begin(), counts.end()));
    for (int r = height; r >= 1; --r) {
        std::cout << (r == height ? std::to_string(maxCount) : std::string())
                  << std::string(r == height ? std::max<int>(0, 9 - static_cast<int>(std::to_string(maxCount).size())) : 9, ' ')
                  << '|' << color::cyan();
        for (int c : counts) {
            double level = static_cast<double>(c) / maxCount * height;
            std::cout << (level >= r - 0.5 ? '#' : ' ');
        }
        std::cout << color::reset() << '\n';
    }
}

inline void heading(const std::string& title) {
    std::cout << '\n' << color::bold() << color::cyan() << std::string(70, '=') << '\n'
              << "  " << title << '\n'
              << std::string(70, '=') << color::reset() << "\n\n";
}

} // namespace brain::plot
