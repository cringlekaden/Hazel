#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/glm.hpp>
// Only the constant-color silhouette fixture: fixed-function perspective
// interpolation precedes the FP32 render target. No PBR/IBL oracle uses this bound.
// See docs/design/metal-default-qualification.md, closure numeric measurements.
static double UnlitColorAllowance(double expected)
{
    return std::max(8e-5 * std::abs(expected), 4.0 * std::numeric_limits<float>::epsilon());
}
static bool UnlitColorMatches(glm::vec3 actual, glm::dvec3 expected)
{
    for(int c=0;c<3;++c)
        if(!std::isfinite(actual[c]) || std::abs(double(actual[c])-expected[c])>UnlitColorAllowance(expected[c]))return false;
    return true;
}
static void UnlitColorPrecisionChecks()
{
    const glm::vec3 red(.8f,.025f,.015f),blue(.015f,.3f,.8f);
    Check(UnlitColorMatches(red,glm::dvec3(red)),"Exact unlit source color rejected");
    // Independent IEEE binary16 round-to-nearest values: this still rejects a
    // half-float radiance attachment rather than quietly accepting lost precision.
    Check(!UnlitColorMatches({.7998046875f,.024993896484375f,.0149993896484375f},glm::dvec3(red)),"Unlit allowance concealed FP16 radiance quantization");
    Check(!UnlitColorMatches(glm::vec3(0),glm::dvec3(red)) && !UnlitColorMatches(blue,glm::dvec3(red)),"Unlit allowance concealed a hole or neighboring surface");
    Check(!UnlitColorMatches(red*(254.f/255)+blue/255.f,glm::dvec3(red)),"Unlit allowance concealed 1/255 neighboring coverage blend");
    Check(!UnlitColorMatches(red*1.001f,glm::dvec3(red)),"Unlit allowance concealed a 0.1 percent radiance gain regression");
    glm::vec3 encoded;
    for(int c=0;c<3;++c)encoded[c]=float(red[c]<=.0031308 ? 12.92*red[c] : 1.055*std::pow(double(red[c]),1.0/2.4)-.055);
    Check(!UnlitColorMatches(encoded,glm::dvec3(red)),"Unlit allowance concealed an sRGB/radiance encoding error");
    auto invalid=red;invalid.x=std::numeric_limits<float>::quiet_NaN();
    Check(!UnlitColorMatches(invalid,glm::dvec3(red)),"Unlit allowance concealed NaN");
    invalid.x=std::numeric_limits<float>::infinity();
    Check(!UnlitColorMatches(invalid,glm::dvec3(red)),"Unlit allowance concealed infinity");
}
