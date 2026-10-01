// Real context tests for the actual target MSDF atlas and application-bound cache.
#include "Hazel.h"
#include "Hazel/Renderer/MSDFData.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
template<class Function> static void Reject(Function function, const char* message)
{
    bool caught=false;
    try { function(); } catch (const std::exception&) { caught=true; }
    Check(caught,message);
}
struct Fixture {
    std::filesystem::path Directory;
    Fixture() {
        std::random_device random;
        Directory=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel-font-é-"+std::to_string(random()));
        Check(std::filesystem::create_directory(Directory),"Could not isolate font fixture");
    }
    ~Fixture() { std::error_code error; std::filesystem::remove_all(Directory,error); }
};
static void CheckAtlas(const Hazel::Ref<Hazel::Font>& font)
{
    const auto* data=font->GetMSDFData();
    Check(data && data->Glyphs.size()>100,"Font geometry missing");
    Check(data->FontGeometry.getGlyph('A') && data->FontGeometry.getGlyph(0x00e9),"Latin-1 glyphs lost");
    Check(data->FontGeometry.getMetrics().lineHeight>0,"Font metrics invalid");
    auto texture=font->GetAtlasTexture();
    Check(texture && texture->IsLoaded() && texture->GetWidth()>0 && texture->GetHeight()>0,"Font atlas missing");
    Check(texture->GetSpecification().Format==Hazel::ImageFormat::RGB8 &&
          !texture->GetSpecification().GenerateMips,"MSDF atlas specification changed");
    texture->Bind();
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(texture->GetWidth())*texture->GetHeight()*3);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,bytes.data());
    const auto extrema=std::minmax_element(bytes.begin(),bytes.end());
    Check(*extrema.first<64 && *extrema.second>192,"MSDF atlas GPU data lacks signed-distance range");
    Check(glGetError()==GL_NO_ERROR,"Font atlas OpenGL error");
}
int main()
{
    try {
        Hazel::Log::Init();
        Fixture fixture;
        const auto unicode=fixture.Directory/std::filesystem::u8path("OpenSans-é.ttf");
        std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf",unicode);
        const auto invalid=fixture.Directory/"invalid.ttf";
        { std::ofstream output(invalid,std::ios::binary); output<<"not a font"; }
        for (int iteration=0; iteration<2; ++iteration) {
            std::weak_ptr<Hazel::Font> cache;
            {
                Hazel::ApplicationSpecification specification;
                specification.Name="Migration Font API";
                Hazel::Application application(specification);
                glfwHideWindow(static_cast<GLFWwindow*>(application.GetWindow().GetNativeWindow()));
                std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<"; Version: "<<glGetString(GL_VERSION)<<'\n';
                auto font=Hazel::Font::GetDefault();
                cache=font;
                Check(Hazel::Font::GetDefault()==font,"Default font cache not shared");
                CheckAtlas(font);
                Hazel::TextComponent text;
                Check(text.FontAsset==font,"Target text component lost default font");
                auto other=Hazel::CreateRef<Hazel::Font>(unicode);
                CheckAtlas(other);
                Reject([&]{ Hazel::CreateRef<Hazel::Font>(fixture.Directory/"missing.ttf"); },"Missing font accepted");
                Reject([&]{ Hazel::CreateRef<Hazel::Font>(invalid); },"Invalid font accepted");
                Hazel::NativeScriptComponent native;
                Check(!native.Instance && !native.InstantiateScript && !native.DestroyScript,"Native script defaults uninitialized");
                // All local owners die before application/context teardown. Only the
                // engine-owned default remains until Renderer2D::Shutdown resets it.
            }
            Check(cache.expired(),"Default font survived application/context destruction");
        }
        std::cout<<"PASS: target glyphs/metrics/MSDF GPU atlas, UTF-8 font path, invalid/missing diagnostics, shared default/text component and repeated context-bound font shutdown\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return 1;
    }
}
