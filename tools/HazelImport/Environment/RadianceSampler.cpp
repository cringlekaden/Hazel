#include "RadianceSampler.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Hazel::EnvironmentBake
{
namespace
{
constexpr double Pi = 3.14159265358979323846;

double RadicalInverse(unsigned index, unsigned base)
{
    double fraction = 1, value = 0;
    while (index)
    {
        fraction /= base;
        value += fraction * (index % base);
        index /= base;
    }
    return value;
}
} // namespace

double RadianceSampler::Area(unsigned row) const
{
    return 2 * Pi / m_Image.Width *
        (std::cos(Pi * row / m_Image.Height) - std::cos(Pi * (row + 1) / m_Image.Height));
}

RadianceSampler::RadianceSampler(const RadianceImage &image,
                                 const std::function<bool()> &cancel) : m_Image(image)
{
    image.Validate();
    m_CDF.reserve(size_t(image.Width) * image.Height);
    // A positive 3x3 proposal covers bilinear reconstruction's support, including
    // wrap and poles. It changes sampling probability, never sampled radiance.
    for (unsigned y = 0; y < image.Height; ++y)
    {
        if (cancel && cancel())
            throw std::runtime_error("Environment preparation cancelled; accepted generation retained");
        for (unsigned x = 0; x < image.Width; ++x)
        {
            double weight = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    auto xx = (int(x) + dx + int(image.Width)) % int(image.Width);
                    auto yy = std::clamp(int(y) + dy, 0, int(image.Height) - 1);
                    auto at = (size_t(yy) * image.Width + xx) * 3;
                    weight += .2126 * image.RGB[at] + .7152 * image.RGB[at + 1] +
                              .0722 * image.RGB[at + 2];
                }
            m_Total += weight * Area(y);
            m_CDF.push_back(m_Total);
            auto at = (size_t(y) * image.Width + x) * 3;
            for (int c = 0; c < 3; ++c)
                m_Constant &= image.RGB[at + c] == image.RGB[c];
        }
    }
}

glm::dvec3 RadianceSampler::ConstantValue() const
{
    return {m_Image.RGB[0], m_Image.RGB[1], m_Image.RGB[2]};
}

glm::dvec3 RadianceSampler::Evaluate(glm::dvec3 direction) const
{
    double u = .5 + std::atan2(direction.z, direction.x) / (2 * Pi);
    double v = std::acos(std::clamp(direction.y, -1., 1.)) / Pi;
    double x = u * m_Image.Width - .5, y = v * m_Image.Height - .5;
    int ix = int(std::floor(x)), iy = int(std::floor(y));
    auto pixel = [&](int xx, int yy) {
        xx = (xx % int(m_Image.Width) + int(m_Image.Width)) % int(m_Image.Width);
        yy = std::clamp(yy, 0, int(m_Image.Height) - 1);
        size_t offset = (size_t(yy) * m_Image.Width + xx) * 3;
        return glm::dvec3(m_Image.RGB[offset], m_Image.RGB[offset + 1], m_Image.RGB[offset + 2]);
    };
    return glm::mix(glm::mix(pixel(ix, iy), pixel(ix + 1, iy), x - ix),
                    glm::mix(pixel(ix, iy + 1), pixel(ix + 1, iy + 1), x - ix), y - iy);
}

double RadianceSampler::PDF(glm::dvec3 direction) const
{
    if (!m_Total)
        return 0;
    double u = .5 + std::atan2(direction.z, direction.x) / (2 * Pi);
    double v = std::acos(std::clamp(direction.y, -1., 1.)) / Pi;
    unsigned x = unsigned(std::floor(u * m_Image.Width)) % m_Image.Width;
    unsigned y = std::min(m_Image.Height - 1, unsigned(v * m_Image.Height));
    size_t index = size_t(y) * m_Image.Width + x;
    return (m_CDF[index] - (index ? m_CDF[index - 1] : 0)) / (m_Total * Area(y));
}

std::vector<RadianceSampler::Sample> RadianceSampler::Samples(unsigned count) const
{
    std::vector<Sample> result;
    result.reserve(count);
    if (!m_Total)
        return result;
    for (unsigned i = 0; i < count; ++i)
    {
        double quantile = (i + .5) / count * m_Total;
        size_t index = std::upper_bound(m_CDF.begin(), m_CDF.end(), quantile) - m_CDF.begin();
        index = std::min(index, m_CDF.size() - 1);
        unsigned x = unsigned(index % m_Image.Width), y = unsigned(index / m_Image.Width);
        // Independent local coordinates; cos(theta) is uniform in solid angle.
        double phi = 2 * Pi * ((x + RadicalInverse(i + 1, 2)) / m_Image.Width - .5);
        double cosine = glm::mix(std::cos(Pi * y / m_Image.Height),
                                  std::cos(Pi * (y + 1) / m_Image.Height),
                                  RadicalInverse(i + 1, 3));
        double sine = std::sqrt(std::max(0., 1 - cosine * cosine));
        glm::dvec3 direction(sine * std::cos(phi), cosine, sine * std::sin(phi));
        double pdf = (m_CDF[index] - (index ? m_CDF[index - 1] : 0)) / (m_Total * Area(y));
        result.push_back({direction, Evaluate(direction), pdf});
    }
    return result;
}
} // namespace Hazel::EnvironmentBake
