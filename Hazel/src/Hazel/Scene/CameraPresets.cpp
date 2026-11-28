#include "hzpch.h"
#include "CameraPresets.h"
#include "Components.h"

namespace Hazel
{
void ApplyCameraPreset(CameraComponent& camera, CameraPreset preset)
{
    auto lens = camera.Camera.GetPhysicalCamera().value_or(PhysicalCamera{});
    CameraExposure exposure;
    exposure.Mode = ExposureMode::Automatic;
    lens.Aperture = 4;
    lens.ShutterSeconds = 1.f / 64;
    lens.ISO = 100;
    const bool cinematic = preset == CameraPreset::Cinematic;
    switch (preset) {
        case CameraPreset::AutomaticBalanced: break;
        case CameraPreset::Cinematic:
            lens.Aperture = 2.8f;
            lens.ShutterSeconds = 1.f / 48;
            exposure.BrighteningRate = .6f;
            exposure.DarkeningRate = 2.5f;
            break;
        case CameraPreset::OutdoorDaylight:
            lens.Aperture = 8;
            lens.ShutterSeconds = 1.f / 250;
            exposure.Metering = ExposureMetering::Average;
            exposure.IncludeSky = false;
            break;
        case CameraPreset::LowLight:
            lens.Aperture = 1.8f;
            lens.ShutterSeconds = 1.f / 48;
            lens.ISO = 800;
            exposure.HighPercentile = .82f;
            exposure.BrighteningRate = .55f;
            break;
        case CameraPreset::ManualPhotographic:
            exposure.Mode = ExposureMode::ManualPhysical;
            break;
        default:
            throw std::invalid_argument("Unknown camera preset");
    }
    camera.Camera.SetPhysicalCamera(lens);
    camera.ExposureOverride = exposure;
    camera.BloomOverride = cinematic ? std::optional<bool>(true) : std::nullopt;
    camera.DepthOfField.Enabled = cinematic && camera.Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective;
    camera.DepthOfField.Quality = cinematic ? DepthOfFieldQuality::Cinematic : DepthOfFieldQuality::Balanced;
    camera.MotionBlur = {cinematic, cinematic ? MotionShutterMode::Angle : MotionShutterMode::Duration,
        180, cinematic ? MotionBlurQuality::Cinematic : MotionBlurQuality::Balanced};
}
}
