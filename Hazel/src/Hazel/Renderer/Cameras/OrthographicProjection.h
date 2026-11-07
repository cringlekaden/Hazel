#pragma once
#include <glm/gtc/matrix_transform.hpp>
namespace Hazel {
struct OrthographicProjection {
    static glm::mat4 Matrix(float left,float right,float bottom,float top,float nearClip,float farClip) {
        return glm::ortho(left,right,bottom,top,nearClip,farClip);
    }
};
}
