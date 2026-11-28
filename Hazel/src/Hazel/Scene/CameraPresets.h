#pragma once

namespace Hazel
{
struct CameraComponent;
enum class CameraPreset { AutomaticBalanced, Cinematic, OutdoorDaylight, LowLight, ManualPhotographic };
void ApplyCameraPreset(CameraComponent& camera, CameraPreset preset);
}
