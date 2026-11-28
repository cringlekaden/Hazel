#pragma once

#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Scene/Entity.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace Hazel
{
    // A single periodic track, sampled by time. The host owns playback and input.
    class RuntimeCameraTour
    {
    public:
        explicit RuntimeCameraTour(const std::filesystem::path& path)
        {
            const auto doc = YAML::Load(FileDocument::Read(path));
            DocumentSchema::Keys(doc, {"Version", "Keys"}, "CameraTour");
            const auto keys = doc["Keys"];
            if (doc["Version"].as<unsigned>() != 1 || !keys.IsSequence() ||
                keys.size() < 4 || keys.size() > 256)
                throw std::runtime_error("CameraTour requires version 1 and 4–256 keys");
            for (const auto& row : keys)
            {
                if (!row.IsSequence() || row.size() != 11)
                    throw std::runtime_error("CameraTour key requires 11 values");
                auto number = [&](unsigned index) {
                    const auto value = row[index].as<float>();
                    if (!std::isfinite(value))
                        throw std::runtime_error("CameraTour values must be finite");
                    return value;
                };
                Key key{number(0), {number(1), number(2), number(3)}, {number(4), number(5), number(6)},
                        number(7), number(8), number(9), number(10)};
                if (key.Seconds < .25f || key.Seconds > 60 || key.Focus < .15f || key.Focus > 100 ||
                    key.Focal < 12 || key.Focal > 110 || key.Aperture < .7f || key.Aperture > 32 ||
                    key.Flare < 0 || key.Flare > 1 || glm::length(key.Target - key.Position) < .1f)
                    throw std::runtime_error("CameraTour key outside supported time, sightline or lens range");
                m_Duration += key.Seconds;
                m_Keys.push_back(key);
            }
        }

        double Duration() const { return m_Duration; }
        void Apply(Entity camera, const CameraComponent& baseline, double seconds) const
        {
            auto t = float(std::fmod(seconds, m_Duration));
            if (t < 0) t += float(m_Duration);
            size_t index = 0;
            while (index + 1 < m_Keys.size() && t >= m_Keys[index].Seconds)
                t -= m_Keys[index++].Seconds;
            const auto size = m_Keys.size();
            const auto& a = m_Keys[index];
            const auto& b = m_Keys[(index + 1) % size];
            const auto& before = m_Keys[(index + size - 1) % size];
            const auto& after = m_Keys[(index + 2) % size];
            const float u = t / a.Seconds, u2 = u*u, u3 = u2*u;
            auto spline = [&](auto member) {
                const auto v0 = (b.*member - before.*member) * (a.Seconds / (before.Seconds + a.Seconds));
                const auto v1 = (after.*member - a.*member) * (a.Seconds / (a.Seconds + b.Seconds));
                return (2*u3-3*u2+1)*(a.*member) + (u3-2*u2+u)*v0 +
                       (-2*u3+3*u2)*(b.*member) + (u3-u2)*v1;
            };
            auto& transform = camera.GetComponent<TransformComponent>();
            transform.Translation = spline(&Key::Position);
            const auto forward = spline(&Key::Target) - transform.Translation;
            const auto length = glm::length(forward);
            if (length < .01f || std::abs(forward.y) >= .999f * length)
                throw std::runtime_error("CameraTour interpolated an invalid sightline");
            transform.Rotation = glm::eulerAngles(glm::quatLookAt(forward / length, glm::vec3(0, 1, 0)));
            auto& component = camera.GetComponent<CameraComponent>();
            const float ease = u2 * (3 - 2*u);
            auto lens = baseline.Camera.GetPhysicalCamera().value_or(PhysicalCamera{});
            lens.LensProjection = true;
            lens.FocalLengthMM = glm::mix(a.Focal, b.Focal, ease);
            lens.Aperture = glm::mix(a.Aperture, b.Aperture, ease);
            lens.FocusDistance = 1.f / glm::mix(1.f/a.Focus, 1.f/b.Focus, ease);
            component.Camera.SetPhysicalCamera(lens);
            component.LensEffects = baseline.LensEffects;
            const float flare = glm::mix(a.Flare, b.Flare, ease);
            component.LensEffects.FlareStrength *= flare;
            component.LensEffects.DirtStrength *= flare;
        }
    private:
        struct Key
        {
            float Seconds;
            glm::vec3 Position, Target;
            float Focal, Aperture, Focus, Flare;
        };
        std::vector<Key> m_Keys;
        double m_Duration = 0;
    };
}
