#pragma once
#include "Hazel/Renderer/Resources/GraphicsContext.h"
#include <memory>
struct GLFWwindow;
namespace Hazel {
class MetalDevice;
class MetalContext final : public GraphicsContext {
public:
    explicit MetalContext(GLFWwindow* window);
    ~MetalContext() override;
    void Init() override;
    void SwapBuffers() override;
    void SetVSync(bool enabled);
private:
    struct Surface;
    GLFWwindow* m_Window;
    std::shared_ptr<MetalDevice> m_Device;
    std::unique_ptr<Surface> m_Surface;
};
}
