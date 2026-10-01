// CPU checks for exact target ECS/YAML dependencies and portable camera state.
#include "Hazel/Scene/SceneCamera.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Core/UUID.h"
#include <entt.hpp>
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
static void Finite(const glm::mat4& projection)
{
    for (int column=0; column<4; ++column)
        for (int row=0; row<4; ++row)
            Check(std::isfinite(projection[column][row]), "Camera projection is non-finite");
}
int main()
{
    try {
        Hazel::Log::Init();
        Hazel::SceneCamera camera;
        Finite(camera.GetProjection());
        camera.SetViewportSize(1600,900);
        const auto projection=camera.GetProjection();
        camera.SetViewportSize(0,0);
        Check(camera.GetProjection()==projection, "Minimized viewport changed valid projection");
        camera.SetPerspective(glm::radians(60.0f),0.1f,500.0f);
        Finite(camera.GetProjection());
        Check(camera.GetProjectionType()==Hazel::SceneCamera::ProjectionType::Perspective,
              "Perspective camera state lost");
        camera.SetOrthographic(20,-2,3);
        Finite(camera.GetProjection());
        Check(camera.GetProjectionType()==Hazel::SceneCamera::ProjectionType::Orthographic,
              "Orthographic camera state lost");

        struct ID { std::uint64_t Value; };
        struct Tag { std::string Name; };
        struct Position { float X,Y; };
        entt::registry registry;
        const auto first=registry.create();
        const auto second=registry.create();
        registry.emplace<ID>(first,ID{static_cast<std::uint64_t>(Hazel::UUID())});
        registry.emplace<Tag>(first,Tag{u8"camera-é-\U0001f680"});
        registry.emplace<Position>(first,Position{2,3});
        registry.emplace<Tag>(second,Tag{"other"});
        std::size_t count=0;
        for (const auto entity:registry.view<ID,Tag,Position>()) {
            Check(entity==first, "ECS intersection selected wrong entity");
            ++count;
        }
        Check(count==1, "ECS intersection size differs");
        YAML::Emitter output;
        output << YAML::BeginMap << YAML::Key << "Entity" << YAML::Value << registry.get<ID>(first).Value
               << YAML::Key << "Tag" << YAML::Value << registry.get<Tag>(first).Name
               << YAML::Key << "Position" << YAML::Value << YAML::Flow << YAML::BeginSeq
               << registry.get<Position>(first).X << registry.get<Position>(first).Y << YAML::EndSeq
               << YAML::Key << "ProjectionType" << YAML::Value << static_cast<int>(camera.GetProjectionType())
               << YAML::Key << "Size" << YAML::Value << camera.GetOrthographicSize() << YAML::EndMap;
        Check(output.good(), "YAML emit failed");
        const auto saved=YAML::Load(output.c_str());
        Check(saved["Entity"].as<std::uint64_t>()==registry.get<ID>(first).Value, "YAML UUID round trip failed");
        Check(saved["Tag"].as<std::string>()==registry.get<Tag>(first).Name, "YAML UTF-8 tag lost");
        Check(saved["Position"].size()==2 && saved["Position"][1].as<float>()==3, "YAML component values lost");
        Check(saved["Size"].as<float>()==20 && saved["ProjectionType"].as<int>()==1, "YAML camera values lost");
        registry.destroy(first);
        Check(!registry.valid(first) && registry.valid(second), "ECS destruction damaged another entity");
        const auto replacement=registry.create();
        Check(replacement!=first && !registry.valid(first), "ECS recycled stale handle without generation");
        bool rejected=false;
        try { YAML::Load("Entities: [unterminated"); } catch (const YAML::Exception&) { rejected=true; }
        Check(rejected, "Invalid YAML accepted");
        std::cout << "PASS: finite default/perspective/orthographic/minimized cameras, exact target ECS views/generations/lifetimes and YAML UUID/UTF-8/component/camera round trips\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
