#include "EnvironmentBake.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace Hazel::EnvironmentBake
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
struct Rule
{
    std::vector<glm::dvec2> Nodes, Circle;
};
Rule Quadrature(unsigned order)
{
    Rule rule;
    for (unsigned i = 0; i < order; ++i)
    {
        double z = std::cos(Pi * (i + .75) / (order + .5)), derivative = 0;
        for (unsigned iteration = 0; iteration < 32; ++iteration)
        {
            double p0 = 1, p1 = z;
            for (unsigned n = 2; n <= order; ++n)
            {
                const double p = ((2 * n - 1) * z * p1 - (n - 1) * p0) / n;
                p0 = p1;
                p1 = p;
            }
            derivative = order * (z * p1 - p0) / (z * z - 1);
            const double next = z - p1 / derivative;
            if (std::abs(next - z) < 1e-14) { z = next; break; }
            z = next;
        }
        rule.Nodes.emplace_back((z + 1) * .5, 1 / ((1 - z * z) * derivative * derivative));
    }
    for (unsigned i = 0; i < order * 4; ++i)
    {
        const double phi = 2 * Pi * (i + .5) / (order * 4);
        rule.Circle.emplace_back(std::cos(phi), std::sin(phi));
    }
    return rule;
}
// Upper-light-hemisphere NDF integral. Splitting the radial domain at Lz=0
// resolves tiny smooth-surface losses that an absolute-only VNDF test can miss.
glm::dvec2 IntegrateBoundedNDF(double mu, double roughness, const Rule &rule)
{
    const double alpha = roughness * roughness, alpha2 = alpha * alpha;
    const double sine = std::sqrt(1 - mu * mu), sv = std::sqrt(mu * mu + alpha2 * sine * sine);
    glm::dvec2 sum(0);
    for (auto circle : rule.Circle)
    {
        const double c = sine * circle.x, root = std::hypot(c, mu);
        const double limit = c >= 0 ? (c + root) / mu : mu / (root - c);
        const double cap = limit * limit / (alpha2 + limit * limit);
        for (auto node : rule.Nodes)
        {
            const double u = cap * node.x * (2 - node.x), jacobian = cap * 2 * (1 - node.x);
            const double slope = alpha * std::sqrt(u / (1 - u));
            const double hz = 1 / std::sqrt(1 + slope * slope), vh = (mu + c * slope) * hz;
            const double nl = (mu * (1 - slope * slope) + 2 * c * slope) / (1 + slope * slope);
            if (nl <= 0) continue;
            const double sl = std::sqrt(nl * nl + alpha2 * (1 - nl * nl));
            const double weight = 2 * nl * (mu + c * slope) / (nl * sv + mu * sl);
            const double fc = std::pow(1 - vh, 5);
            sum += glm::dvec2(1 - fc, fc) * (weight * node.y * jacobian / double(rule.Circle.size()));
        }
    }
    return sum;
}
// Production offline integrator: exact correlated-Smith G2/G1 VNDF weight.
// Independent tests evaluate the intrinsic D/V/F and their own quadratures.
glm::dvec2 Integrate(double mu, double roughness, const Rule &rule)
{
    const double alpha = roughness * roughness, alpha2 = alpha * alpha;
    const double vx = std::sqrt(std::max(0., 1 - mu * mu));
    const double sv = std::sqrt(mu * mu + alpha2 * (1 - mu * mu));
    const double sx = alpha * vx / sv, sz = mu / sv, blend = (1 + sz) * .5;
    glm::dvec2 sum(0);
    for (auto node : rule.Nodes)
    {
        // At V=N the radial upper-hemisphere boundary is analytic. Splitting
        // there removes a quadrature kink; no runtime BRDF endpoint is changed.
        if (mu == 1)
        {
            const double boundary = 1 / (1 + alpha2), u = node.x * boundary;
            const double hz = std::sqrt((1 - u) / (1 + (alpha2 - 1) * u));
            const double nl = 2 * hz * hz - 1, sl = std::sqrt(nl * nl + alpha2 * (1 - nl * nl));
            const double oneMinus = 1 - hz, square = oneMinus * oneMinus;
            const double fc = square * square * oneMinus;
            sum += glm::dvec2(1 - fc, fc) * (2 * nl / (nl + sl) * node.y * boundary);
            continue;
        }
        // u=1-(1-t)^2 resolves the VNDF disk-edge square-root tail.
        // Include its Jacobian; increasing uniform-area samples alone converges slowly.
        const double radius = std::sqrt(node.x * (2 - node.x));
        const double radialWeight = node.y * 2 * (1 - node.x);
        for (auto circle : rule.Circle)
        {
            const double x = radius * circle.x;
            const double y = (1 - blend) * std::sqrt(std::max(0., 1 - x * x)) + blend * radius * circle.y;
            const double z = std::sqrt(std::max(0., 1 - x * x - y * y));
            double hx = alpha * (-sz * y + sx * z), hy = alpha * x, hz = sx * y + sz * z;
            hz = std::max(0., hz);
            const double length = std::sqrt(hx * hx + hy * hy + hz * hz);
            hx /= length; hz /= length;
            const double vh = std::clamp(vx * hx + mu * hz, 0., 1.), nl = 2 * vh * hz - mu;
            if (nl <= 0) continue;
            const double sl = std::sqrt(nl * nl + alpha2 * (1 - nl * nl));
            const double weight = (mu + sv) * nl / (sv * nl + mu * sl);
            const double oneMinus = 1 - vh, square = oneMinus * oneMinus;
            const double fc = square * square * oneMinus;
            sum += glm::dvec2(1 - fc, fc) * (weight * radialWeight / double(rule.Circle.size()));
        }
    }
    return sum;
}
}
LightingImage IntegrateRuntimeBRDF(unsigned side, const Cancel &cancel)
{
    if (side != 128 && side != 256 && side != 512)
        throw std::runtime_error("Runtime BRDF side must be 128, 256 or 512");
    std::vector<Rule> rules;
    for (unsigned n = 32; n <= 4096; n *= 2) rules.push_back(Quadrature(n));
    LightingImage image; image.Kind = LightingImageKind::BRDF; image.Side = side;
    image.Mips.emplace_back(size_t(side) * side * 2);
    std::atomic<unsigned> row{0}, completed{0};
    std::mutex mutex;
    std::exception_ptr failure;
    unsigned maximumOrder = 0;
    double maximumDelta = 0;
    auto worker = [&] {
        try
        {
            while (true)
            {
                const unsigned y = row.fetch_add(1);
                if (y >= side) break;
                if (cancel && cancel()) throw std::runtime_error("Runtime BRDF preparation cancelled");
                const double rough = .045 + .955 * double(y) / (side - 1);
                for (unsigned x = 0; x < side; ++x)
                {
                    const double mu = std::pow(double(x) / (side - 1), 4);
                    // NDF weights are well conditioned here; VNDF handles the
                    // grazing endpoint without a 1/mu singularity. This selects
                    // quadrature only, never a different BRDF or runtime clamp.
                    auto evaluate = [&](const Rule &rule) { return mu >= rough * rough ?
                        IntegrateBoundedNDF(mu, rough, rule) : Integrate(mu, rough, rule); };
                    auto previous = evaluate(rules[0]);
                    bool converged = false;
                    double lastDelta = 0;
                    for (size_t level = 1; level < rules.size(); ++level)
                    {
                        const auto value = evaluate(rules[level]);
                        const double delta = std::max(std::abs(value.x - previous.x), std::abs(value.y - previous.y));
                        lastDelta = delta;
                        const double missing = std::max(0., 1 - value.x - value.y);
                        const double missingDelta = std::abs((value.x + value.y) - (previous.x + previous.y));
                        if (delta <= 1e-7 && missingDelta <= std::max(1e-10, missing * 1e-3))
                        {
                            if (value.x < 0 || value.y < 0 || value.x + value.y > 1 + 1e-10)
                                throw std::runtime_error("Runtime BRDF integration created energy");
                            const size_t p = (size_t(y) * side + x) * 2;
                            const float a = float(value.x);
                            float b = float(value.y);
                            // Directed final conversion keeps convex interpolated E <= 1.
                            // This bounds only FP32 rounding of newly generated data.
                            if (double(a) + b > 1) b = std::nextafter(float(1 - double(a)), 0.f);
                            image.Mips[0][p] = a; image.Mips[0][p + 1] = b;
                            {
                                std::lock_guard<std::mutex> lock(mutex);
                                maximumOrder = std::max(maximumOrder, 32u << unsigned(level));
                                maximumDelta = std::max(maximumDelta, delta);
                            }
                            converged = true;
                            break;
                        }
                        previous = value;
                    }
                    if (!converged) throw std::runtime_error("Runtime BRDF quadrature failed to converge at " + std::to_string(x) + "," + std::to_string(y) + " delta=" + std::to_string(lastDelta * 1e9) + "e-9");
                }
                const unsigned done = completed.fetch_add(1) + 1;
                if (done % 16 == 0)
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    std::cout << "Runtime BRDF rows " << done << '/' << side << std::endl;
                }
            }
        }
        catch (...)
        {
            row.store(side);
            std::lock_guard<std::mutex> lock(mutex);
            if (!failure) failure = std::current_exception();
        }
    };
    std::vector<std::thread> workers;
    // Fixed deterministic per-texel reduction; worker count does not affect bytes.
    try { for (unsigned i = 0; i < 6; ++i) workers.emplace_back(worker); }
    catch (...) { row.store(side); for (auto &thread : workers) thread.join(); throw; }
    for (auto &thread : workers) thread.join();
    if (failure) std::rethrow_exception(failure);
    image.Validate();
    std::cout << "RuntimeBRDF_v2 side=" << side << " maximum_order=" << maximumOrder
              << " maximum_delta=" << maximumDelta << std::endl;
    return image;
}
} // namespace Hazel::EnvironmentBake
