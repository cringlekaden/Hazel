#include "Hazel/Core/Resources.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Renderer/Buffer.h"
#include "Hazel/Renderer/Camera.h"
#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/UniformBuffer.h"
#include "Hazel/Renderer/VertexArray.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

static void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void CheckAttribute(GLuint index, GLint size, GLint type, GLint integer,
                           GLint stride, GLint divisor, std::uintptr_t offset)
{
    GLint value = 0;
    for (const auto& query : std::array<std::array<GLint, 2>, 6>{{
        {GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_TRUE},
        {GL_VERTEX_ATTRIB_ARRAY_SIZE, size},
        {GL_VERTEX_ATTRIB_ARRAY_TYPE, type},
        {GL_VERTEX_ATTRIB_ARRAY_INTEGER, integer},
        {GL_VERTEX_ATTRIB_ARRAY_STRIDE, stride},
        {GL_VERTEX_ATTRIB_ARRAY_DIVISOR, divisor}}})
    {
        glGetVertexAttribiv(index, static_cast<GLenum>(query[0]), &value);
        Check(value == query[1], "Unexpected vertex attribute state");
    }
    void* pointer = nullptr;
    glGetVertexAttribPointerv(index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
    Check(reinterpret_cast<std::uintptr_t>(pointer) == offset, "Incorrect matrix/attribute offset");
}

static void RunRendererChecks()
{
    using namespace Hazel;
    Camera camera(glm::mat4(1.0f));
    Check(camera.GetProjection() == glm::mat4(1.0f), "Camera projection differs");
    Check(Camera().GetProjection() == glm::mat4(1.0f), "Default camera projection differs");

    GLuint sentinel = 0;
    glGenBuffers(1, &sentinel);
    glBindBuffer(GL_UNIFORM_BUFFER, sentinel);
    glBufferData(GL_UNIFORM_BUFFER, 16, nullptr, GL_DYNAMIC_DRAW);
    auto uniform = UniformBuffer::Create(80, 3);
    GLint binding = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &binding);
    Check(static_cast<GLuint>(binding) == sentinel, "UBO constructor changed generic binding");
    GLint indexedBinding = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 3, &indexedBinding);
    Check(indexedBinding != 0 && static_cast<GLuint>(indexedBinding) != sentinel, "UBO indexed binding missing");

    std::array<float, 20> expected{};
    std::memcpy(expected.data(), glm::value_ptr(camera.GetProjection()), 64);
    expected[16] = 0.25f;
    expected[17] = 0.5f;
    expected[18] = 0.75f;
    expected[19] = 1.0f;
    uniform->SetData(expected.data(), 64);
    uniform->SetData(expected.data() + 16, 16, 64);
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &binding);
    Check(static_cast<GLuint>(binding) == sentinel, "UBO update changed generic binding");
    glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(indexedBinding));
    std::array<float, 20> actual{};
    glGetBufferSubData(GL_UNIFORM_BUFFER, 0, 80, actual.data());
    Check(actual == expected, "UBO full/offset upload readback differs");
    glBindBuffer(GL_UNIFORM_BUFFER, sentinel);

    auto vao = VertexArray::Create();
    float positions[] = {-1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f};
    auto vertices = VertexBuffer::Create(positions, sizeof(positions));
    vertices->SetLayout({{ShaderDataType::Float3, "Position"}});
    vao->AddVertexBuffer(vertices);

    BufferLayout layout = {{ShaderDataType::Int, "EntityID"},
                           {ShaderDataType::Mat4, "Transform"},
                           {ShaderDataType::Bool, "Enabled"},
                           {ShaderDataType::Mat3, "Basis"}};
    Check(layout.GetStride() == 105, "Packed layout stride differs");
    std::vector<std::uint8_t> packed(layout.GetStride() * 3, 0);
    const std::int32_t entityID = 73;
    const glm::mat4 transform(1.0f);
    const glm::mat3 basis(1.0f);
    for (std::uint32_t i = 0; i < 3; ++i)
    {
        auto* record = packed.data() + i * layout.GetStride();
        std::memcpy(record, &entityID, 4);
        std::memcpy(record + 4, glm::value_ptr(transform), 64);
        record[68] = 1;
        std::memcpy(record + 69, glm::value_ptr(basis), 36);
    }
    auto instances = VertexBuffer::Create(static_cast<std::uint32_t>(packed.size()));
    instances->SetData(packed.data(), static_cast<std::uint32_t>(packed.size()));
    instances->SetLayout(layout);
    vao->AddVertexBuffer(instances);
    CheckAttribute(0, 3, GL_FLOAT, GL_FALSE, 12, 0, 0);
    CheckAttribute(1, 1, GL_INT, GL_TRUE, 105, 0, 0);
    for (GLuint i = 0; i < 4; ++i)
        CheckAttribute(2 + i, 4, GL_FLOAT, GL_FALSE, 105, 1, 4 + 16 * i);
    CheckAttribute(6, 1, GL_UNSIGNED_BYTE, GL_TRUE, 105, 0, 68);
    for (GLuint i = 0; i < 3; ++i)
        CheckAttribute(7 + i, 3, GL_FLOAT, GL_FALSE, 105, 1, 69 + 12 * i);
    Check(glGetError() == GL_NO_ERROR, "Buffer/attribute setup raised an OpenGL error");

    auto shader = Shader::Create("MigrationRendererSmoke", R"(
#version 420 core
layout(location=0) in vec3 a_Position;
layout(location=1) in int a_EntityID;
layout(location=2) in mat4 a_Transform;
layout(location=6) in int a_Enabled;
layout(location=7) in mat3 a_Basis;
layout(std140, binding=3) uniform Camera { mat4 u_ViewProjection; vec4 u_Color; };
flat out int v_EntityID;
out vec4 v_Color;
void main() {
    gl_Position = u_ViewProjection * a_Transform * vec4(a_Basis * a_Position, 1.0);
    v_EntityID = a_EntityID;
    v_Color = u_Color * float(a_Enabled);
})", R"(
#version 420 core
flat in int v_EntityID;
in vec4 v_Color;
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_EntityID;
void main() { o_Color = v_Color; o_EntityID = v_EntityID; }
)");

    GLuint framebuffer = 0;
    GLuint attachments[2]{};
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(2, attachments);
    for (int i = 0; i < 2; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, attachments[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, i == 0 ? GL_RGBA8 : GL_R32I, 16, 16, 0,
                     i == 0 ? GL_RGBA : GL_RED_INTEGER, i == 0 ? GL_UNSIGNED_BYTE : GL_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, attachments[i], 0);
    }
    const GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);
    Check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Test framebuffer incomplete");
    glViewport(0, 0, 16, 16);
    shader->Bind();
    vao->Bind();
    glDrawArraysInstanced(GL_TRIANGLES, 0, 3, 1);
    std::array<std::uint8_t, 4> pixel{};
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    const std::array<int, 4> expectedPixel = {64, 128, 191, 255};
    for (std::size_t i = 0; i < pixel.size(); ++i)
        Check(std::abs(static_cast<int>(pixel[i]) - expectedPixel[i]) <= 1, "Rendered UBO color readback differs");
    GLint pickedEntity = -1;
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(8, 8, 1, 1, GL_RED_INTEGER, GL_INT, &pickedEntity);
    Check(pickedEntity == entityID, "Rendered integer entity ID differs");
    Check(glGetError() == GL_NO_ERROR, "Rendering/readback raised an OpenGL error");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(2, attachments);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(1, &sentinel);
    std::cout << "PASS: Camera, UBO binding/upload/readback, float/int/bool/Mat3/Mat4 attributes, instanced color/entity rendering\n";
}

int main()
{
    Hazel::Log::Init(); Hazel::Resources::Configure(Hazel::Resources::Defaults("RendererSmoke"));
    glfwSetErrorCallback([](int code, const char* message) { std::cerr << "GLFW " << code << ": " << message << '\n'; });
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(16, 16, "Migration verification", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    int result = 0;
    try
    {
        Check(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GLAD load failed");
        std::cout << "GPU: " << glGetString(GL_RENDERER) << "\nOpenGL: " << glGetString(GL_VERSION) << '\n';
        RunRendererChecks();
        Check(glGetError() == GL_NO_ERROR, "Resource destruction raised an OpenGL error");
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        result = 1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
