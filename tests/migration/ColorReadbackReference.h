#pragma once
#include <cmath>
// Independent readback oracle, not the production transfer helper. Hardware
// SRGB8 decode is approximate: Mesa's documented polynomial maps byte 128 to
// .216638 before half storage (.216553 observed), versus IEC .2158605. Require
// recovery of the original byte within half a code value. Keep the tighter
// 6e-4 half-float bounds for authored linear RGB, data, alpha and compositing.
// https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/auxiliary/gallivm/lp_bld_format_srgb.c
static bool SRGB8ReadbackMatches(double linear, unsigned encoded) {
    if(!std::isfinite(linear) || linear<0 || linear>1)return false;
    const double srgb=linear<=.0031308?12.92*linear:1.055*std::pow(linear,1/2.4)-.055;
    return std::abs(srgb*255-encoded)<=.5;
}
