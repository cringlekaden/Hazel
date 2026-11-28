#include "Hazel/Scene/CameraPresets.h"
// CPU checks for exact target ECS/YAML dependencies and portable camera state.
#include "Hazel/Scene/SceneCamera.h"
#include "Hazel/Renderer/Cameras/CameraDefocus.h"
#include "Hazel/Renderer/Cameras/CameraShutter.h"
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Project/RuntimeStorage.h"
#include "Hazel/Core/FileSystem.h"
#include <fstream>
#include "Hazel/Core/Log.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Assets/SpriteSheetDocument.h"
#include "Hazel/Assets/FontFamilyDocument.h"
#include <fstream>
#include <algorithm>
#include <entt.hpp>
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <limits>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
static void PhysicalCameraContracts()
{
    using namespace Hazel;
    PhysicalCamera lens;
    const auto close = [](double x, double y) { return std::abs(x - y) <= 2e-6; };
    Check(close(lens.EV100(), 10), "Reference camera is not EV100 10");
    for (float stops : {-16.f, -1.f, 0.f, 1.f, 16.f})
        Check(lens.Log2ExposureGain(stops) == stops, "Reference camera changed authored compensation");
    lens.Aperture = 8;
    Check(close(lens.EV100(), 12) && close(lens.Log2ExposureGain(1), -1), "Aperture reciprocity or compensation sign differs");
    lens.ShutterSeconds = 1.f / 128;
    Check(close(lens.EV100(), 13), "Shutter reciprocity differs");
    lens.ISO = 800;
    Check(close(lens.EV100(), 10), "ISO reciprocity differs");
    lens.Aperture = 2.8f;
    lens.ShutterSeconds = 1.f / 125;
    lens.ISO = 400;
    Check(close(lens.EV100(), std::log(245.) / std::log(2.)), "Non-dyadic EV100 differs");
    Check(lens.Log2ExposureGain(-16) >= -16 && lens.Log2ExposureGain(16) == 16, "Presentation gain escaped its limits");

    lens.LensProjection = true;
    auto scene = CreateRef<Scene>();
    auto entity = scene->CreateEntityWithUUID(17, "Physical camera");
    auto& camera = entity.AddComponent<CameraComponent>().Camera;
    camera.SetViewportSize(1600, 900);
    camera.SetPerspective(glm::radians(60.f), .25f, 320);
    const auto compatibilityProjection = camera.GetProjection();
    const auto before = SceneSerializer(scene).CaptureAuthoredState();
    const auto legacyText = SceneSerializer(scene).SerializeText();
    camera.SetPhysicalCamera(lens);
    const double aspect = 16. / 9;
    Check(close(lens.HorizontalFOV(aspect), 2 * std::atan(.36)) &&
          close(lens.VerticalFOV(aspect), 2 * std::atan(.2025)), "Wide sensor crop differs");
    Check(close(lens.VerticalFOV(1), 2 * std::atan(.24)) &&
          close(lens.HorizontalFOV(.5), 2 * std::atan(.12)), "Tall sensor crop differs");
    const auto projection = camera.GetProjection();
    Check(close(2 * std::atan(1. / projection[0][0]), 2 * std::atan(.36)) &&
          close(2 * std::atan(1. / projection[1][1]), 2 * std::atan(.2025)), "Camera projection ignored active sensor");
    Check(projection[2] == compatibilityProjection[2] && projection[3] == compatibilityProjection[3],
          "Physical framing changed clipping");
    Check(SceneSerializer(scene).CaptureAuthoredState() != before, "Physical settings did not dirty authored state");
    const auto text = SceneSerializer(scene).SerializeText();
    auto loaded = CreateRef<Scene>();
    SceneSerializer reader(loaded);
    Check(reader.DeserializeText(text), "Physical camera document failed to load");
    Check(reader.CaptureAuthoredState() == SceneSerializer(scene).CaptureAuthoredState(),
          "Physical camera serialization lost data");
    auto invalid = YAML::Load(text);
    invalid["Entities"][0]["CameraComponent"]["Camera"]["PhysicalLens"]["Version"] = 2;
    Check(!reader.DeserializeText(YAML::Dump(invalid)) && reader.SerializeText() == text,
          "Unknown physical lens version mutated the loaded scene");
    auto copy = Scene::Copy(scene);
    Check(SceneSerializer(copy).CaptureAuthoredState() == SceneSerializer(scene).CaptureAuthoredState(),
          "Play copy lost physical camera settings");
    Check(SceneSerializer(loaded).DeserializeText(legacyText) &&
          !loaded->GetEntityByUUID(17).GetComponent<CameraComponent>().Camera.GetPhysicalCamera(),
          "Older camera unexpectedly acquired a physical lens");
    camera.SetPhysicalCamera(std::nullopt);
    Check(camera.GetProjection() == compatibilityProjection &&
          SceneSerializer(scene).CaptureAuthoredState() == before, "Removing lens did not restore camera state");
    lens.FocusDistance = .01f;
    bool rejected = false;
    try { camera.SetPhysicalCamera(lens); }
    catch (const std::runtime_error&) { rejected = true; }
    Check(rejected && !camera.GetPhysicalCamera(), "Invalid optical focus mutated the camera");
    std::cout << "PASS: physical camera EV100, sensor crop, clipping, persistence, compatibility and Play copy\n";
}
static void CameraDefocusContracts()
{
    using namespace Hazel;
    PhysicalCamera lens;
    lens.LensProjection = true;
    lens.FocalLengthMM = 85;
    lens.SensorWidthMM = 36;
    lens.SensorHeightMM = 24;
    lens.Aperture = 2.f;
    lens.FocusDistance = 4.f;
    constexpr unsigned height = 2160;
    for (const auto extent : {glm::uvec2(3840, height), glm::uvec2(1440, height)}) {
        SceneCamera camera;
        camera.SetViewportSize(extent.x, extent.y);
        camera.SetPerspective(glm::radians(50.f), .01f, 1000);
        const double physicalGateMM = std::min(24.0, 36.0 * height / extent.x);
        for (bool physicalFraming : {false, true}) {
            lens.LensProjection = physicalFraming;
            camera.SetPhysicalCamera(lens);
            const auto& projection = camera.GetProjection();
            const auto defocus = CameraDefocus::FromPerspective(lens, projection, height);
            const double gateMM = physicalFraming ? physicalGateMM : 170 * std::tan(glm::radians(25.0));
            // The edge ray runs from the pupil rim to the conjugate image
            // plane. Intersect it with the fixed sensor for each object.
            const auto imagePlane = [](double distanceMM) { return 1 / (1. / 85 - 1 / distanceMM); };
            const double sensorPlane = imagePlane(4000);
            const double pupilRadius = 85. / 4;
            for (double object : {.5, 1., 3.8, 4., 4.2, 10., 80., 900.}) {
                const auto clip = glm::dmat4(projection) * glm::dvec4(0, 0, -object, 1);
                const double depth = .5 * (clip.z / clip.w + 1);
                const double radius = pupilRadius * (sensorPlane / imagePlane(object * 1000) - 1) * height / gateMM;
                Check(std::abs(defocus.RadiusPixels(depth) - radius) < 5e-4,
                      "Projected signed defocus differs from independent FP64 edge-ray intersection");
                const float shaderRadius = (float(depth) - float(defocus.FocusWindowDepth)) *
                    float(defocus.RadiusPixelsPerDepth);
                Check(std::abs(shaderRadius - radius) < .03,
                      "Projected signed defocus lost precision when packed for the shader");
            }
        }
    }
    auto authored = CreateRef<Scene>();
    auto entity = authored->CreateEntity("Cinematic depth of field");
    auto& camera = entity.AddComponent<CameraComponent>();
    auto subject = authored->CreateEntity("Tracked focus subject");
    subject.GetComponent<TransformComponent>().Translation = {7, 2, -14};
    authored->Reparent(subject, entity);
    camera.Camera.SetPhysicalCamera(lens);
    camera.DepthOfField = {true, DepthOfFieldQuality::Cinematic, 7, .2f, .3f, subject.GetUUID()};
    const auto text = SceneSerializer(authored).SerializeText();
    auto loaded = CreateRef<Scene>();
    SceneSerializer reader(loaded);
    Check(reader.DeserializeText(text) && reader.CaptureAuthoredState() == SceneSerializer(authored).CaptureAuthoredState(),
          "Depth of field settings did not round-trip");
    auto invalid = YAML::Load(text);
    invalid["Entities"][0]["CameraComponent"]["DepthOfField"]["ApertureBlades"] = 2;
    Check(!reader.DeserializeText(YAML::Dump(invalid)) && reader.SerializeText() == text,
          "Invalid blade count mutated the loaded camera");
    SceneCameraView view;
    view.DepthOfField = camera.DepthOfField;
    Check(SceneRenderer::Collect(*authored, view).Camera.FocusSubjectDepth == 14.f,
          "Tracked focus must use axial depth at the entity world pivot");
    auto duplicate = authored->DuplicateEntity(entity);
    const auto children = authored->GetChildren(duplicate.GetUUID());
    Check(children.size() == 1 && duplicate.GetComponent<CameraComponent>().DepthOfField.TrackedSubject == children.front(),
          "Camera subtree duplication lost its local focus reference");
    std::cout << "PASS: signed perspective circle of confusion versus independent FP64 edge-ray intersection\n";
}
static void CameraShutterContracts()
{
    using namespace Hazel;
    PhysicalCamera lens;
    lens.ShutterSeconds = 1.f / 64;
    MotionBlurSettings shutter;
    Check(shutter.Fraction(lens, 1. / 60) == 0, "Photography settings enabled shutter blur");
    shutter.Enabled = true;
    constexpr double pixelsPerSecond = 384;
    for (double fps : {24., 30., 60., 120.}) {
        const double dt = 1 / fps;
        const double imageDelta = 384 * dt;
        const double measuredSupport = imageDelta * shutter.Fraction(lens, dt) * .5;
        Check(std::abs(measuredSupport - 3) < 1e-12,
              "An absolute shutter should preserve smear length under a frame-rate change");
    }
    shutter.Shutter = MotionShutterMode::Angle;
    shutter.ShutterAngleDegrees = 180;
    for (double fps : {24., 30., 60., 120.}) {
        // Independently integrate two times at the quarter-frame endpoints.
        const double dt = 1 / fps;
        const double width = pixelsPerSecond * dt * shutter.Fraction(lens, dt);
        const double reference = pixelsPerSecond * dt * .25 - pixelsPerSecond * (-dt * .25);
        Check(std::abs(width - reference) < 1e-12, "Angle shutter support differs from its time-domain ray");
    }
    Check(shutter.Fraction(lens, 0) == 0 &&
          shutter.Fraction(lens, std::numeric_limits<double>::infinity()) == 0,
          "Discontinuous elapsed time retained a shutter support");
    const auto originalEV = lens.EV100();
    shutter.ShutterAngleDegrees = 0;
    Check(shutter.Fraction(lens, 1. / 120) == 0 && lens.EV100() == originalEV,
          "Motion angle changed physical exposure");
    auto scene = CreateRef<Scene>();
    auto camera = scene->CreateEntity("Motion shutter");
    shutter.ShutterAngleDegrees = 144;
    shutter.Quality = MotionBlurQuality::Cinematic;
    camera.AddComponent<CameraComponent>().MotionBlur = shutter;
    auto copy = CreateRef<Scene>();
    SceneSerializer reader(copy);
    Check(reader.DeserializeText(SceneSerializer(scene).SerializeText()) &&
          copy->GetEntityByUUID(camera.GetUUID()).GetComponent<CameraComponent>().MotionBlur == shutter,
          "Camera shutter settings did not round-trip");
    std::cout << "PASS: motion shutter duration, angle, invalid elapsed time and persistence\n";
}
static void CameraLensContracts()
{
    using namespace Hazel;
    auto scene = CreateRef<Scene>();
    auto camera = scene->CreateEntity("Lens effects");
    auto& lens = camera.AddComponent<CameraComponent>().LensEffects;
    lens.FlareStrength = .3f;
    lens.AnamorphicStreak = .2f;
    lens.DirtTexture = "CameraStudy/Glass.png";
    lens.DirtStrength = .4f;
    lens.Distortion = -.12f;
    lens.Grain = .01f;
    const auto text = SceneSerializer(scene).SerializeText();
    auto copy = CreateRef<Scene>();
    SceneSerializer reader(copy);
    Check(reader.DeserializeText(text) &&
          copy->GetEntityByUUID(camera.GetUUID()).GetComponent<CameraComponent>().LensEffects == lens,
          "Camera lens settings did not round-trip");
    auto invalid = YAML::Load(text);
    invalid["Entities"][0]["CameraComponent"]["LensEffects"]["DirtTexture"] = "../Outside.png";
    Check(!reader.DeserializeText(YAML::Dump(invalid)) && reader.SerializeText() == text,
          "Escaping lens dirt path mutated the camera");
    std::cout << "PASS: lens optics scene round-trip and transactional asset-path rejection\n";
}
static void CameraGradeContracts()
{
    using namespace Hazel;
    auto scene = CreateRef<Scene>();
    auto entity = scene->CreateEntity("Camera look");
    auto& camera = entity.AddComponent<CameraComponent>();
    camera.Camera.SetPerspective(glm::radians(60.f), .1f, 500);
    const auto originalFOV = camera.Camera.GetPerspectiveVerticalFOV();

    Check(ColorGradeSettings{}.WhiteBalance() == glm::dmat3(1), "D65 must produce an exact identity");
    camera.ColorGrade.WhiteKelvin = 2856;
    // Independently integrate the CIE standard observer for a 2856 K blackbody.
    const auto gray = camera.ColorGrade.WhiteBalance() * glm::dvec3(.369025803, .165221047, .046678129);
    Check(glm::length(gray - glm::dvec3(.2)) < .002, "White balance did not neutralize the thermal reference");
    camera.ColorGrade.WhiteKelvin = 4950;
    camera.ColorGrade.TintUV = .003f;
    camera.ColorGrade.LUT = "CameraStudy/Look.cube";
    camera.ColorGrade.LUTStrength = .48f;
    for (auto preset : {CameraPreset::AutomaticBalanced, CameraPreset::Cinematic,
             CameraPreset::OutdoorDaylight, CameraPreset::LowLight, CameraPreset::ManualPhotographic}) {
        const auto color = camera.ColorGrade;
        ApplyCameraPreset(camera, preset);
        const bool motion = preset == CameraPreset::Cinematic;
        Check(camera.Camera.GetPerspectiveVerticalFOV() == originalFOV && camera.ColorGrade == color,
              "Preset changed the projection or authored color");
        Check(camera.DepthOfField.Enabled == motion && camera.MotionBlur.Enabled == motion &&
              (camera.ExposureOverride->Mode == ExposureMode::ManualPhysical) ==
                   (preset == CameraPreset::ManualPhotographic),
              "Preset drove unintended exposure or optics");
        camera.Camera.GetPhysicalCamera()->Validate();
        camera.ExposureOverride->Validate();
    }
    camera.BloomOverride = false;
    const auto text = SceneSerializer(scene).SerializeText();
    auto copy = CreateRef<Scene>();
    SceneSerializer reader(copy);
    Check(reader.DeserializeText(text), "Color grade scene could not be loaded");
    const auto& loaded = copy->GetEntityByUUID(entity.GetUUID()).GetComponent<CameraComponent>();
    Check(loaded.ColorGrade == camera.ColorGrade && loaded.BloomOverride == std::optional<bool>(false),
          "Color grade or explicit bloom override changed across serialization");
    auto invalid = YAML::Load(text);
    invalid["Entities"][0]["CameraComponent"]["ColorGrade"]["LUT"] = "../Outside.cube";
    Check(!reader.DeserializeText(YAML::Dump(invalid)) && reader.SerializeText() == text,
          "Out-of-project camera LUT mutated the loaded scene");
    std::cout << "PASS: camera white balance, five presets and transactional grading persistence\n";
}
static void CameraExposureMigration()
{
    using namespace Hazel;
    const auto legacy = YAML::Load("Mode: SceneReferred\nBackground: [0,0,0,1]\nExposure: -1.25\n");
    const auto migrated = SceneDefaults::ReadPresentation(legacy);
    Check(migrated.Exposure.CompensationStops == -1.25f, "Legacy compensation was reinterpreted");
    const auto encoded = SceneDefaults::Encode(migrated);
    Check(!encoded["Exposure"] && encoded["CameraExposure"]["Version"].as<unsigned>() == 1 &&
          encoded["CameraExposure"]["Mode"].as<std::string>() == "ManualPhysical" &&
          SceneDefaults::ReadPresentation(encoded) == migrated, "Canonical exposure round-trip failed");

    auto rejects = [](YAML::Node node) {
        try { SceneDefaults::ReadPresentation(node); }
        catch (const std::exception&) { return true; }
        return false;
    };
    auto ambiguous = YAML::Clone(encoded);
    ambiguous["Exposure"] = -1.25f;
    Check(rejects(ambiguous), "Conflicting exposure representations accepted");
    auto unknown = YAML::Clone(encoded);
    unknown["CameraExposure"]["Version"] = 2;
    Check(rejects(unknown), "Future exposure schema accepted");
    auto malformed = YAML::Clone(encoded);
    malformed["CameraExposure"]["BrighteningRate"] = "fast";
    Check(rejects(malformed), "Malformed adaptation rate silently used a default");
    auto display = YAML::Clone(encoded);
    display["Mode"] = "DisplayReferred";
    Check(rejects(display), "Display-referred exposure accepted");
    Check(SceneDefaults::DirectDefaults().Presentation.Exposure.Mode == ExposureMode::Automatic,
          "New HDR project defaults are not automatic");
    auto scene = CreateRef<Scene>();
    scene->Presentation() = migrated;
    auto camera = scene->CreateEntity("Camera metering override");
    CameraExposure meter;
    meter.Mode = ExposureMode::Automatic;
    meter.Metering = ExposureMetering::Spot;
    meter.LowPercentile = .21f;
    meter.HighPercentile = .82f;
    meter.SpotCenter = {.23f, .63f};
    meter.SpotRadius = .08f;
    meter.IncludeSky = false;
    meter.Locked = true;
    meter.MinGainStops = -3.f;
    meter.MaxGainStops = 6.f;
    meter.BrighteningRate = .25f;
    meter.DarkeningRate = 6.f;
    camera.AddComponent<CameraComponent>().ExposureOverride = meter;
    auto saved = SceneSerializer(scene).SerializeText();
    auto loaded = CreateRef<Scene>();
    Check(SceneSerializer(loaded).DeserializeText(saved), "Camera override did not reopen");
    auto found = loaded->GetEntityByUUID(camera.GetUUID());
    Check(found && found.GetComponent<CameraComponent>().ExposureOverride == std::optional<CameraExposure>(meter) &&
          SceneSerializer(scene).CaptureAuthoredState() == SceneSerializer(loaded).CaptureAuthoredState(),
          "Camera override authored values changed across round trip");
    std::cout << "PASS: legacy exposure compensation migration and single canonical camera schema\n";
}
static void LocalLightContracts() {
    using namespace Hazel;
    auto scene=CreateRef<Scene>();auto e=scene->CreateEntityWithUUID(4242,"Point and spot");
    e.GetComponent<TransformComponent>().Translation={2,3,4};e.GetComponent<TransformComponent>().Rotation={.2f,.4f,.1f};
    e.GetComponent<TransformComponent>().Scale={2,3,4};
    auto &point=e.AddComponent<PointLightComponent>();point.Range=17;point.Intensity=23;point.Color={.2f,.4f,.6f};
    point.CastShadows=true;point.ShadowResolution=512;
    auto &spot=e.AddComponent<SpotLightComponent>();spot.Range=31;spot.InnerCone=.2f;spot.OuterCone=.8f;spot.Enabled=false;spot.CastShadows=true;spot.ShadowResolution=1024;
    const auto text=SceneSerializer(scene).SerializeText();auto loaded=CreateRef<Scene>();
    Check(SceneSerializer(loaded).DeserializeText(text),"Local light round trip rejected");
    auto assertLights=[&](Entity candidate) {
        Check(candidate.HasComponent<PointLightComponent>() && candidate.HasComponent<SpotLightComponent>(),"Local components lost");
        Check(candidate.GetComponent<PointLightComponent>().Range==17 && candidate.GetComponent<PointLightComponent>().Intensity==23 &&
            candidate.GetComponent<PointLightComponent>().CastShadows && candidate.GetComponent<PointLightComponent>().ShadowResolution==512 &&
            candidate.GetComponent<SpotLightComponent>().Range==31 && candidate.GetComponent<SpotLightComponent>().InnerCone==.2f &&
            !candidate.GetComponent<SpotLightComponent>().Enabled && candidate.GetComponent<SpotLightComponent>().CastShadows &&
            candidate.GetComponent<SpotLightComponent>().ShadowResolution==1024,"Local authored values lost");
    };
    auto legacyText = text;
    for (auto field : {"    CastShadows:", "    ShadowResolution:"}) for (int light = 0; light < 2; ++light) {
        const auto mark = legacyText.find(field);
        Check(mark != std::string::npos, "Serialized local shadow field missing");
        const auto start = legacyText.rfind('\n', mark) + 1;
        legacyText.erase(start, legacyText.find('\n', mark) + 1 - start);
    }
    auto legacy = CreateRef<Scene>();
    Check(SceneSerializer(legacy).DeserializeText(legacyText), "Legacy spotlight schema rejected");
    const auto& legacyPoint = legacy->GetEntityByUUID(4242).GetComponent<PointLightComponent>();
    Check(!legacyPoint.CastShadows && legacyPoint.ShadowResolution == 256, "Legacy point shadow defaults differ");
    const auto& legacySpot = legacy->GetEntityByUUID(4242).GetComponent<SpotLightComponent>();
    Check(!legacySpot.CastShadows && legacySpot.ShadowResolution == 512, "Legacy spotlight shadow defaults differ");
    assertLights(loaded->GetEntityByUUID(4242));auto copy=Scene::Copy(scene);assertLights(copy->GetEntityByUUID(4242));
    auto duplicate=scene->DuplicateEntity(e);assertLights(duplicate);Check(duplicate.GetUUID()!=e.GetUUID(),"Duplicated light identity reused");
    auto prefab=CreateRef<Scene>();
    Check(SceneSerializer(prefab).DeserializeText(Prefab::Serialize({},scene,e,true),true),"Local prefab round trip rejected");
    assertLights(Prefab::GetEntity(prefab));
    const auto position=LightPosition(scene->GetWorldTransform(e)),travel=DirectionalTravel(scene->GetWorldTransform(e));
    Check(glm::length(position-glm::vec3(2,3,4))<1e-6f && std::abs(glm::length(travel)-1)<1e-6f,"Local transform convention failed");
    for(float range:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        PointLight bad;bad.Range=range;bool rejected=false;try{bad.Validate();}catch(const std::exception&){rejected=true;}
        Check(rejected,"Invalid local range accepted");
    }
    SpotLight bad;bad.InnerCone=bad.OuterCone;bool rejected=false;try{bad.Validate();}catch(const std::exception&){rejected=true;}
    Check(rejected,"Degenerate spot penumbra accepted");
    auto many=CreateRef<Scene>();
    for(uint64_t i=320;i>0;--i)many->CreateEntityWithUUID(i,"Local capacity").AddComponent<PointLightComponent>();
    const auto collected=SceneRenderer::Collect(*many,SceneCameraView{});
    Check(collected.LocalLights.size()==320 && collected.LocalLights.front().Identity==1 && collected.LocalLights.back().Identity==320,"Local collection bound/order failed");
    many->CreateEntityWithUUID(321,"Over capacity").AddComponent<PointLightComponent>();rejected=false;
    try{SceneRenderer::Collect(*many,SceneCameraView{});}catch(const std::exception&){rejected=true;}
    Check(rejected,"Local capacity silently truncated lights");
    std::cout<<"PASS: point/spot schema, round trip, copy, duplicate, prefab, transforms and validation\n";
}
static void AuthoredComparisonContracts() {
    using namespace Hazel;
    auto scene=CreateRef<Scene>();auto e=scene->CreateEntity("Authored comparison");
    e.AddComponent<SpriteRendererComponent>();e.AddComponent<SpriteAnimationComponent>();
    e.AddComponent<AudioSourceComponent>();e.AddComponent<CameraComponent>();e.AddComponent<ScriptComponent>();
    e.AddComponent<Rigidbody2DComponent>();e.AddComponent<BoxCollider2DComponent>();e.AddComponent<CircleCollider2DComponent>();
    e.AddComponent<DirectionalLightComponent>();e.AddComponent<PointLightComponent>();e.AddComponent<SpotLightComponent>();
    e.AddComponent<TextComponent>(TextComponent{});e.AddComponent<CircleRendererComponent>();
    SceneSerializer serializer(scene);const auto baseline=serializer.CaptureAuthoredState();
    auto changed=[&](auto& field,auto value){const auto old=field;field=value;Check(serializer.CaptureAuthoredState()!=baseline,"Persisted edit did not dirty exact authored state");field=old;Check(serializer.CaptureAuthoredState()==baseline,"Revert did not restore exact authored state");};
    changed(e.GetComponent<TextComponent>().Font.Family,std::filesystem::path("Fonts/Owned.hfont"));
    changed(e.GetComponent<TextComponent>().Font.Style,FontStyle::Bold);
    changed(e.GetComponent<TagComponent>().Tag,std::string("Changed"));
    changed(e.GetComponent<TransformComponent>().Translation,glm::vec3(1,2,0));
    changed(e.GetComponent<SpriteRendererComponent>().Color,glm::vec4(.4f));
    changed(e.GetComponent<SpriteAnimationComponent>().Speed,2.0);
    changed(e.GetComponent<AudioSourceComponent>().Gain,.8f);
    changed(e.GetComponent<CameraComponent>().Primary,false);
    changed(e.GetComponent<ScriptComponent>().ClassName,std::string("Missing.ButEditable"));
    changed(e.GetComponent<Rigidbody2DComponent>().GravityScale,0.f);
    changed(e.GetComponent<BoxCollider2DComponent>().Restitution,.3f);
    changed(e.GetComponent<CircleCollider2DComponent>().Radius,.7f);
    changed(e.GetComponent<TextComponent>().TextString,std::string("Different prose"));
    changed(e.GetComponent<CircleRendererComponent>().Thickness,.5f);
    changed(e.GetComponent<PointLightComponent>().Color,glm::vec3(.2f,.3f,.4f));
    changed(e.GetComponent<PointLightComponent>().Intensity,12.f);
    changed(e.GetComponent<PointLightComponent>().Range,7.f);
    changed(e.GetComponent<PointLightComponent>().Enabled,false);
    changed(e.GetComponent<PointLightComponent>().CastShadows,true);
    changed(e.GetComponent<PointLightComponent>().ShadowResolution,512u);
    changed(e.GetComponent<SpotLightComponent>().InnerCone,.2f);
    changed(e.GetComponent<SpotLightComponent>().OuterCone,.7f);
    changed(e.GetComponent<SpotLightComponent>().Range,8.f);
    changed(e.GetComponent<SpotLightComponent>().CastShadows,true);
    changed(e.GetComponent<SpotLightComponent>().ShadowResolution,256u);
    changed(e.GetComponent<DirectionalLightComponent>().Color, glm::vec3(.2f, .4f, .6f));
    changed(e.GetComponent<DirectionalLightComponent>().Intensity, 3.f);
    changed(e.GetComponent<DirectionalLightComponent>().Enabled, false);
    auto& animation=e.GetComponent<SpriteAnimationComponent>();animation.Error="Preview failure";animation.Initialized=true;
    Check(serializer.CaptureAuthoredState()==baseline,"Transient preview state dirtied authored content");
    e.GetComponent<TransformComponent>().Scale.x=0;
    Check(serializer.CaptureAuthoredState()!=baseline,"Invalid draft was hidden from dirty comparison");
    bool rejected=false;try{serializer.SerializeText();}catch(const std::exception&){rejected=true;}
    Check(rejected,"Fast dirty comparison bypassed save validation");
}
static void ScriptRequirementContracts() {
    using namespace Hazel;
    auto scene=CreateRef<Scene>();
    Check(RuntimeSession::RequiredScriptClasses(scene).empty(),"Native content required an assembly");
    auto entity=scene->CreateEntity("Unassigned script");entity.AddComponent<ScriptComponent>();
    Check(RuntimeSession::RequiredScriptClasses(scene).empty(),"Empty class assignment required an assembly");
    entity.GetComponent<ScriptComponent>().ClassName="Unavailable.Required";
    auto copy=scene->DuplicateEntity(entity);
    Check(RuntimeSession::RequiredScriptClasses(scene)==std::set<std::string>{"Unavailable.Required"},"Assigned requirements were omitted or duplicated");
    RuntimeSession::Validate(scene,false);
    bool rejected=false;try{RuntimeSession::Validate(scene);}catch(const std::exception&){rejected=true;}
    Check(rejected,"Unavailable assigned script was Play-ready");
    std::cout<<"PASS: shared native assigned-script requirements and independent content-only/metadata readiness\n";
}
#include "EditorDocumentChecks.h"
#include "EditorStateChecks.h"
#include "ProjectCreationChecks.h"
#include "RendererPolicyChecks.h"
#include "HierarchyChecks.h"
#include "AuthoredReferenceChecks.h"
#include "AssetCookingChecks.h"
#include "ImportCookingChecks.h"
#include "Hazel/Core/OperationProgress.h"
#include "ImportPublicationChecks.h"
#include "EnvironmentBakeChecks.h"
static void Finite(const glm::mat4& projection)
{
    for (int column=0; column<4; ++column)
        for (int row=0; row<4; ++row)
            Check(std::isfinite(projection[column][row]), "Camera projection is non-finite");
}
static void RecoveryContracts()
{
    using namespace Hazel;
    const auto root=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel recovery space-é-"+std::to_string(uint64_t(UUID())));
    std::filesystem::create_directories(root);
    const auto file=root/std::filesystem::u8path(u8"source é.hazel");
    const std::string original=u8"SceneVersion: 3\nPresentation: {Mode: DisplayReferred, Background: [0, 0, 0, 1]}\nScene: Missing é\nEntities:\n  - Entity: 72\n    Relationship: {Parent: 0, Order: 0}\n    TagComponent: {Tag: Unresolved}\n    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TexturePath: Textures/missing é.png\n      TilingFactor: 1\n";
    FileSystem::WriteNewFile(file,original);
    FileDocument owner;owner.Open(file);
    auto scene=CreateRef<Scene>();
    SceneSerializer loader(scene,root,true,CreateRef<ProjectAssets>(root));
    Check(loader.DeserializeText(owner.Original()),"Missing texture was not editable");
    Check(loader.Report().Migration && loader.Report().State==DocumentLoadState::EditableWithProblems,
          "Missing resource/default changes were not identified");
    auto report=loader.Report();report.Saved();
    Check(!report.Migration && report.State==DocumentLoadState::EditableWithProblems && report.Problems.size()==1,
          "Saving migration incorrectly resolved a missing resource or retained obsolete encoding warnings");
    const auto saved=loader.SerializeText();
    Check(saved.find(u8"Textures/missing é.png")!=std::string::npos && saved.find("SceneVersion: 3")!=std::string::npos,
          "Unresolved texture reference or scene version lost");
    owner.PreserveOriginal();owner.Save(saved,root/"recovery");
    Check(FileDocument::Read(owner.Backup())==original,"First recovery Save lost original bytes");
    Check(loader.DeserializeText(saved),"Unresolved saved scene failed reopen");
    const auto draft=loader.SerializeText();
    for(const auto& text:{std::string("SceneVersion: 99\nScene: Future\nEntities: []\n"),
                        std::string("Scene: Unknown\nAlien: 4\nEntities: []\n"),
                        std::string("Scene: Duplicate\nScene: Second\n"),
                        std::string("Scene: X\nEntities: [{Entity: 72, TransformComponent: {Translation: [0, 0, 0], Rotation: [0, 0, 0], Scale: [1, 1, 1], Alien: true}}]\n")}) {
        Check(!loader.DeserializeText(text) && loader.SerializeText()==draft,"Rejected schema changed valid scene");
    }
    FileSystem::WriteFileAtomically(file,[](auto& out){out<<"External content";});
    bool conflict=false;try{owner.Save(saved,root/"recovery");}catch(const std::exception&){conflict=true;}
    Check(conflict && FileDocument::Read(file)=="External content" && loader.SerializeText()==draft,
          "Save conflict overwrote disk or draft");
    std::filesystem::remove(file);conflict=false;
    try{owner.Save(saved,root/"recovery");}catch(const std::exception&){conflict=true;}
    Check(conflict && !std::filesystem::exists(file),"Missing accepted file was silently recreated");
    auto config=CreateRef<Project>();config->GetConfig().Name="Retained";
    const auto descriptor=root/"future.hproj";
    FileSystem::WriteNewFile(descriptor,"Project: {Version: 99}\n");
    ProjectSerializer projectLoader(config);
    Check(!projectLoader.Deserialize(descriptor) && config->GetConfig().Name=="Retained" &&
          projectLoader.Report().Error.find("Unsupported Project Version")!=std::string::npos,
          "Future project changed accepted configuration or lacked version diagnostic");
    // An exclusive prefab write must preserve an existing destination.
    auto entity=scene->GetEntityByUUID(72);
    Prefab::Save(root,"retained.hprefab",scene,entity,true,WriteMode::CreateNew);
    const auto prefab=FileDocument::Read(root/"retained.hprefab");
    const auto beforePrefab=loader.SerializeText();
    Check(!loader.DeserializeText(prefab) && loader.SerializeText()==beforePrefab,"Scene reader silently stripped prefab metadata");
    auto prefabCandidate=CreateRef<Scene>();
    Check(SceneSerializer(prefabCandidate,root,true).DeserializeText(prefab,true),"Explicit prefab reader rejected known prefab content");
    bool exclusive=false;try{Prefab::Save(root,"retained.hprefab",scene,entity,true,WriteMode::CreateNew);}catch(const std::exception&){exclusive=true;}
    Check(exclusive && FileDocument::Read(root/"retained.hprefab")==prefab,"Prefab creation replaced another file");
    unsigned char image[18+16]{};image[2]=2;image[12]=image[14]=2;image[16]=32;image[17]=0x20;
    {std::ofstream out(root/"texture.tga",std::ios::binary);out.write(reinterpret_cast<const char*>(image),sizeof(image));}
    // Audit a real whole-texture source through the same UTF-8 file service used
    // by scene Open; narrow YAML::LoadFile paths fail on Windows in this root.
    FileSystem::WriteNewFile(root/"textured.hazel", "SceneVersion: 3\nPresentation: {Mode: DisplayReferred, Background: [0, 0, 0, 1]}\nScene: Closure\nEntities:\n  - Entity: 91\n    Relationship: {Parent: 0, Order: 0}\n    TagComponent: {Tag: Textured}\n    SpriteRendererComponent: {Color: [1, 1, 1, 1], TexturePath: texture.tga, TilingFactor: 1}\n");
    const auto closure=AuditSpriteAssets(root,{"textured.hazel"});
    Check(std::find(closure.begin(),closure.end(),std::filesystem::path("texture.tga"))!=closure.end(),"Unicode scene closure omitted whole-texture dependency");
    const std::string sheetText="SpriteSheet: {Version: 1, Texture: texture.tga, TextureSize: [2, 2], Filter: Nearest, Regions: [{ID: '0000000000000001', Name: First, Rect: [0, 0, 2, 2]}]}\n";
    FileSystem::WriteNewFile(root/"legacy.hsprites",sheetText);
    auto retainedAssets=CreateRef<ProjectAssets>(root);
    try{retainedAssets->Sheet("repaired.hsprites");}catch(const std::exception&){}
    FileSystem::WriteNewFile(root/"repaired.hsprites",sheetText);
    auto candidateAssets=CreateRef<ProjectAssets>(root);
    Check(candidateAssets->Sheet("repaired.hsprites")->Region(1).Name=="First","Fresh Open candidate reused a stale missing-sheet failure");
    bool oldFailure=false;try{retainedAssets->Sheet("repaired.hsprites");}catch(const std::exception&){oldFailure=true;}
    Check(oldFailure,"Candidate validation altered the retained asset cache");
    retainedAssets->Refresh();
    Check(retainedAssets->Sheet("repaired.hsprites")->Region(1).Name=="First","Accepted refresh did not resolve an externally repaired sheet");
    SpriteSheetDocument sheet(CreateRef<ProjectAssets>(root));sheet.Open("legacy.hsprites");
    Check(sheet.File().NeedsBackup(),"Known sheet defaults/Filter migration was hidden");
    sheet.Draft().Regions.front().Name="Edited";sheet.Changed();
    bool failedSave=false;try{sheet.Save(root/"texture.tga");}catch(const std::exception&){failedSave=true;}
    Check(failedSave && sheet.Dirty() && FileDocument::Read(root/"legacy.hsprites")==sheetText,"Failed original backup overwrote sheet or lost draft");
    sheet.Save(root/"recovery");
    Check(!sheet.Dirty() && FileDocument::Read(sheet.File().Backup())==sheetText,"Sheet migration lost original or dirty semantics");
    for(unsigned i=0;i<22;++i) {
        const auto item=root/("bounded-"+std::to_string(i));FileSystem::WriteNewFile(item,"Original");
        FileDocument original;original.Open(item,true);original.Save("Saved",root/"recovery");
    }
    unsigned retained=0;for(const auto& item:std::filesystem::directory_iterator(root/"recovery"))if(item.path().extension()==".original")++retained;
    Check(retained==20,"Original recovery history was not bounded");
    std::filesystem::remove_all(root);
    std::cout<<"PASS: CPU production recovery load/round-trip, unknown/future/duplicate schema rejection, original backups, missing/external save conflicts and exclusive prefab publication\n";
}
static void StorageChecks() {
    using namespace Hazel;
    auto root=std::filesystem::temp_directory_path()/("hazel-save-"+std::to_string(uint64_t(UUID())));
    struct Cleanup{std::filesystem::path P;~Cleanup(){std::error_code ec;std::filesystem::remove_all(P,ec);}} cleanup{root};
    RuntimeStorage player;player.Configure(root,"Keeper",true);
    Check(player.Read("progress").empty(),"Missing save isn't empty");
    player.Write("progress","v1:harbor é");player.Write("progress","v1:relay é");
    RuntimeStorage reopened;reopened.Configure(root,"Keeper",true);
    Check(reopened.Read("progress")=="v1:relay é","Atomic replace/UTF-8 reopen failed");
    RuntimeStorage editor;editor.Configure(root,"Keeper",false);
    Check(editor.Read("progress").empty(),"Editor loaded player save");editor.Write("progress","editor-only");
    Check(editor.Read("progress")=="editor-only" && reopened.Read("progress")=="v1:relay é","Editor changed player save");
    editor.Clear();editor.Configure(root,"Keeper",false);Check(editor.Read("progress").empty(),"Stop retained editor progress");
    RuntimeStorage other;other.Configure(root,"Other",true);Check(other.Read("progress").empty(),"Projects share progress");
    for(auto slot:{"../progress","/absolute","C:drive","..","CON/path","NUL","com1","trail."}) {
        bool rejected=false;try{reopened.Write(slot,"data");}catch(const std::exception&){rejected=true;}Check(rejected,"Unsafe slot accepted");
    }
    bool rejected=false;try{reopened.Write("progress",std::string(RuntimeStorage::MaximumPayload+1,'x'));}catch(const std::exception&){rejected=true;}
    Check(rejected && reopened.Read("progress")=="v1:relay é","Oversize write damaged progress");
    auto file=root/"Keeper/slot-progress.save";
    FileSystem::WriteFileAtomically(file,[](auto& out){out<<"Version: 99\nOwner: Keeper\nSlot: progress\nPayload: newer\n";});
    rejected=false;try{reopened.Read("progress");}catch(const std::exception&){rejected=true;}Check(rejected,"Future save accepted");
    {std::ifstream in(file);std::string bytes((std::istreambuf_iterator<char>(in)),{});Check(bytes.find("99")!=std::string::npos,"Future file erased");}
    FileSystem::WriteFileAtomically(file,[](auto& out){out<<"[malformed";});
    rejected=false;try{reopened.Read("progress");}catch(const std::exception&){rejected=true;}Check(rejected,"Corrupt save accepted");
    std::cout<<"PASS: project save isolation, editor overlay, UTF-8 atomic replace, traversal/size/corrupt/future rejection\n";
}

static void CPUFontFamilyDocument() {
    using namespace Hazel;const auto root=std::filesystem::temp_directory_path()/("hazel-cpu-family-"+std::to_string(uint64_t(UUID())));
    std::filesystem::create_directories(root);
    struct Cleanup{std::filesystem::path Root;~Cleanup(){std::error_code ec;std::filesystem::remove_all(Root,ec);}} cleanup{root};
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf",root/"face.ttf");
    std::filesystem::copy_file("assets/fonts/opensans/LICENSE.txt",root/"LICENSE.txt");
    FontFamilyDefinition definition;definition.Faces[0]={"face.ttf",{"LICENSE.txt"}};definition.Required="café";
    {auto assets=CreateRef<ProjectAssets>(root);FontFamilyDocument document(assets);document.Create("CPU.hfont",definition);
     document.Draft().Name="CPU saved";document.Changed();document.Save();
     Check(!document.Dirty() && AuditFontFamily(root,"CPU.hfont").size()==3,"CPU-only family authoring/closure failed");}
    std::cout<<"PASS: font-family create/save/coverage/closure/retirement without a renderer or application\n";
}
int main()
{
    try {
        EnvironmentBakeChecks();
        StorageChecks();
        AssetCookingChecks();ImportCookingChecks();
        Hazel::Log::Init();
        PhysicalCameraContracts();CameraDefocusContracts();CameraShutterContracts();CameraLensContracts();CameraGradeContracts();CameraExposureMigration();LocalLightContracts();AuthoredComparisonContracts();ImportPublicationChecks();
        CPUFontFamilyDocument();
        Hazel::EditorDocumentChecks();
        Hazel::EditorStateChecks();
        ScriptRequirementContracts();Hazel::ProjectCreationChecks();Hazel::RendererPolicyChecks(); Hazel::HierarchyChecks(); Hazel::SubtreePrefabChecks();AuthoredReferenceChecks();
        RecoveryContracts();
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
