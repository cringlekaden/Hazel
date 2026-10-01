#pragma once
// Target yaml-cpp 25be1f2 emitterutils.cpp relies on transitive integer headers.
// Supply its declared types externally; preserve the pinned vendor source.
#include <cstdint>
using std::uint16_t;
using std::uint32_t;
