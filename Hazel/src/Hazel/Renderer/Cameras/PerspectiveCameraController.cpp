#include "hzpch.h"
#include "PerspectiveCameraController.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
namespace Hazel {
glm::quat PerspectiveCameraController::Orientation() const {return glm::quat(glm::vec3(-Pitch,-Yaw,0));}
glm::vec3 PerspectiveCameraController::Forward() const {return Orientation()*glm::vec3(0,0,-1);}
glm::vec3 PerspectiveCameraController::Right() const {return Orientation()*glm::vec3(1,0,0);}
glm::vec3 PerspectiveCameraController::Up() const {return Orientation()*glm::vec3(0,1,0);}
glm::vec3 PerspectiveCameraController::Position() const {return Focus-Forward()*Distance;}
void PerspectiveCameraController::Orbit(glm::vec2 points) {
    Yaw+=points.x*.005f;Pitch=std::clamp(Pitch+points.y*.005f,-1.55334f,1.55334f);
}
void PerspectiveCameraController::Pan(glm::vec2 points,float verticalFOV,float height) {
    if(height>0)Focus+=(-Right()*points.x+Up()*points.y)*(2*Distance*std::tan(verticalFOV*.5f)/height);
}
void PerspectiveCameraController::Dolly(float steps) {Distance=std::clamp(Distance*std::exp(-steps*.15f),.05f,100000.f);}
void PerspectiveCameraController::Fly(glm::vec2 look,glm::vec3 movement,float seconds,float speed) {
    auto position=Position();Orbit(look);
    float length=glm::length(movement);if(length>1)movement/=length;
    position+=(Right()*movement.x+glm::vec3(0,1,0)*movement.y+Forward()*movement.z)*std::max(0.f,seconds)*speed;
    Focus=position+Forward()*Distance;
}
void PerspectiveCameraController::SetPose(glm::vec3 position,glm::quat orientation,float distance) {
    auto forward=glm::normalize(orientation*glm::vec3(0,0,-1));
    Pitch=std::asin(std::clamp(-forward.y,-1.f,1.f));Yaw=std::atan2(forward.x,-forward.z);
    Distance=std::clamp(distance,.05f,100000.f);Focus=position+Forward()*Distance;
}
}
