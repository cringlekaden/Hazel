#pragma once
static void ImageFitChecks()
{
    Rect box{{10,20},{100,80}};
    auto stretch=FitImage(box,{40,10},ImageFit::Stretch,{.5f,.5f});
    Check(stretch.Visual.Size==box.Size&&stretch.UV.Size==glm::vec2(1),"Stretch image contract changed");
    auto contain=FitImage(box,{40,10},ImageFit::Contain,{.5f,.5f});
    Check(contain.Visual.Size==glm::vec2(100,25)&&contain.Visual.Position==glm::vec2(10,47.5f),"Contain aspect/alignment failed");
    auto cover=FitImage(box,{40,10},ImageFit::Cover,{1,0});
    Check(cover.Visual.Position==box.Position&&cover.Visual.Size==box.Size&&Same(cover.UV.Size.x,.3125f)&&Same(cover.UV.Position.x,.6875f),"Cover crop/alignment failed");
    auto native=FitImage(box,{40,10},ImageFit::Native,{1,1});
    Check(native.Visual.Size==glm::vec2(40,10)&&native.Visual.Position==glm::vec2(70,90),"Native image scaled or ignored alignment");
    native=FitImage(box,{200,160},ImageFit::Native,{.5f,.5f});
    Check(native.Visual.Size==box.Size&&native.UV.Position==glm::vec2(.25f)&&native.UV.Size==glm::vec2(.5f),"Native oversized image failed to crop at one pixel per logical unit");
    Check(FitImage(box,{0,10},ImageFit::Contain,{}).Visual.Size==glm::vec2(0),"Missing intrinsic image produced fitted geometry");
    Check(FitImage({{},{}},{40,10},ImageFit::Cover,{}).Visual.Size==glm::vec2(0),"Zero-size fitting produced geometry");
    Reject([&]{FitImage(box,{40,10},ImageFit(99),{});});Reject([&]{FitImage(box,{NAN,10},ImageFit::Cover,{});});Reject([&]{FitImage(box,{40,10},ImageFit::Cover,{2,0});});
    auto extreme=FitImage({{},{1e20f,1e-20f}},{1e-20f,1e20f},ImageFit::Cover,{.5f,.5f});
    Check(extreme.Visual.Size.x==1e20f&&std::isfinite(extreme.UV.Size.x),"Extreme finite aspect fit overflowed");
    std::cout<<"PASS: independent image box/visual/crop, Stretch/Contain/Cover/Native, alignment and zero/invalid aspect policies\n";
}
