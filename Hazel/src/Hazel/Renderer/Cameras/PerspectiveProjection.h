#pragma once
#include <glm/gtc/matrix_transform.hpp>
namespace Hazel {
// Angles are radians, aspect is logical width/height. No pose or input ownership.
struct PerspectiveProjection {
    static glm::mat4 Matrix(float verticalFOV, float aspect, float nearClip, float farClip) {
        return glm::perspective(verticalFOV, aspect, nearClip, farClip);
    }
};
}
