#pragma once

// Tiny ANSI color helper. Disable with --no-color.
namespace brain::color {

inline bool enabled = true;

inline const char* code(const char* c) { return enabled ? c : ""; }
inline const char* reset()  { return code("\033[0m"); }
inline const char* bold()   { return code("\033[1m"); }
inline const char* dim()    { return code("\033[2m"); }
inline const char* red()    { return code("\033[31m"); }
inline const char* green()  { return code("\033[32m"); }
inline const char* yellow() { return code("\033[33m"); }
inline const char* blue()   { return code("\033[34m"); }
inline const char* magenta(){ return code("\033[35m"); }
inline const char* cyan()   { return code("\033[36m"); }

} // namespace brain::color
