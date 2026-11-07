#pragma once
namespace Hazel {
static void CameraNavigationChecks(){
    NavigationCamera camera(45,2,.1f,1000);camera.SetViewportSize(800,400);
    ViewportNavigation route;NavigationInput input;input.OverImage=true;input.Alt=true;input.Left=true;
    route.Update(camera,input,1.f/60);auto initial=camera.GetPosition();
    for(int i=1;i<=400;++i){input.Pointer.x=i*2.f;input.OverImage=i<20;route.Update(camera,input,1.f/60);}
    Check(route.Active()&&camera.GetYaw()>glm::radians(180.f),"Uninterrupted orbit stalled outside image or below 90 degrees");
    input.Left=false;route.Update(camera,input,0);Check(!route.Active(),"Orbit failed to release");
    input.Space=true;input.Alt=false;input.Left=true;input.OverImage=true;route.Update(camera,input,0);auto yaw=camera.GetYaw();
    input.Pointer.x+=100;route.Update(camera,input,0);Check(camera.GetYaw()>yaw+.49f,"Space-drag alternate orbit failed");
    input.Focused=false;route.Update(camera,input,0);Check(!route.Active(),"Focus loss retained camera ownership");
    input.Focused=true;input.Pointer.x+=10000;route.Update(camera,input,0);Check(!route.Active(),"Focus return resumed a held gesture");
    input.Left=false;route.Update(camera,input,0);input.Left=true;route.Update(camera,input,0);Check(camera.GetYaw()<yaw+1,"Re-entry pointer jumped");
    input.Blocked=true;route.Update(camera,input,0);Check(!route.Active(),"Widget acquired input without camera release");
    PerspectiveCameraController a,b;a.SetPose({1,2,3},glm::quat(1,0,0,0));b=a;
    for(int i=0;i<60;++i)a.Fly({0,0},{0,0,1},1.f/60,3);
    for(int i=0;i<120;++i)b.Fly({0,0},{0,0,1},1.f/120,3);
    Check(glm::length(a.Position()-b.Position())<.0001f,"Fly speed depends on frame rate");
    auto position=a.Position();a.Fly({80,0},{0,0,0},0,3);Check(glm::length(position-a.Position())<.0001f,"Entering look moved the eye/pivot");
    auto planar=camera.PlanarFocus();camera.SetNavigation(EditorNavigation::Planar);camera.Pan({20,10});camera.Scroll(1);auto saved=camera.PlanarFocus();camera.SetNavigation(EditorNavigation::Orbit);camera.SetNavigation(EditorNavigation::Planar);
    Check(saved==camera.PlanarFocus()&&saved!=planar,"Navigation switch lost independent planar pose");
    camera.FrameBounds({1,2,3},{1,2,3});Check(std::isfinite(camera.GetViewProjection()[0][0]),"Degenerate bounds made projection invalid");
    std::cout<<"PASS: uninterrupted 229-degree orbit, outside-image retention, Space alternate, focus cancellation/re-entry, frame-rate independent fly and separate planar pose\n";
}

}
