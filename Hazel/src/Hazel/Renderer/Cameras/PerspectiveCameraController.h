#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
namespace Hazel
{
// Reusable orbit/fly pose and movement policy. Hosts supply logical-point deltas
// and seconds. No GLFW, editor, bindings, projection or authored camera dependency.
class PerspectiveCameraController
{
  public:
    glm::vec3 Focus{0};
    float Pitch = 0, Yaw = 0, Distance = 10, Roll = 0;
    glm::quat Orientation() const;
    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const;
    glm::vec3 Position() const;
    void Orbit(glm::vec2 points);
    void Pan(glm::vec2 points, float verticalFOV, float logicalHeight);
    void Dolly(float steps);
    void Fly(glm::vec2 look, glm::vec3 localMovement, float seconds, float speed);
    void SetPose(glm::vec3 position, glm::quat orientation, float pivotDistance = 10);
};
} // namespace Hazel
