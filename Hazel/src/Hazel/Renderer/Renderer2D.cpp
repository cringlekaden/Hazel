#include "hzpch.h"
#include "Hazel/Renderer/Font.h"

#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/VertexArray.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Hazel {

    struct QuadVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        glm::vec2 TexCoord;
        float TexIndex;
        float TilingFactor;
    };

    struct Renderer2DData
    {
        static constexpr std::uint32_t MaxQuads = 20000;
        static constexpr std::uint32_t MaxVertices = MaxQuads * 4;
        static constexpr std::uint32_t MaxIndices = MaxQuads * 6;
        static constexpr std::uint32_t MaxTextureSlots = 16;
        Ref<VertexArray> QuadVertexArray;
        Ref<VertexBuffer> QuadVertexBuffer;
        Ref<Shader> TextureShader;
        Ref<Texture2D> WhiteTexture;
        std::uint32_t QuadIndexCount = 0;
        Scope<QuadVertex[]> QuadVertexBufferBase;
        QuadVertex* QuadVertexBufferPtr = nullptr;
        std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
        std::uint32_t TextureSlotIndex = 1;
        std::array<glm::vec4, 4> QuadVertexPositions;
        Renderer2D::Statistics Stats;
    };

    static Scope<Renderer2DData> s_Data;

    void Renderer2D::Init()
    {
        HZ_PROFILE_FUNCTION();
        s_Data = CreateScope<Renderer2DData>();
        s_Data->QuadVertexArray = VertexArray::Create();
        s_Data->QuadVertexBuffer = VertexBuffer::Create(
            static_cast<std::uint32_t>(
                Renderer2DData::MaxVertices * sizeof(QuadVertex)));
        s_Data->QuadVertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float,  "a_TexIndex" },
            { ShaderDataType::Float,  "a_TilingFactor" }
        });
        s_Data->QuadVertexArray->AddVertexBuffer(
            s_Data->QuadVertexBuffer);
        s_Data->QuadVertexBufferBase =
            CreateScope<QuadVertex[]>(
                Renderer2DData::MaxVertices);
        Scope<std::uint32_t[]> quadIndices =
            CreateScope<std::uint32_t[]>(
                Renderer2DData::MaxIndices);
        std::uint32_t offset = 0;
        for (std::uint32_t i = 0; i < Renderer2DData::MaxIndices; i += 6)
        {
            quadIndices[i + 0] = offset + 0;
            quadIndices[i + 1] = offset + 1;
            quadIndices[i + 2] = offset + 2;
            quadIndices[i + 3] = offset + 2;
            quadIndices[i + 4] = offset + 3;
            quadIndices[i + 5] = offset + 0;
            offset += 4;
        }
        Ref<IndexBuffer> quadIB = IndexBuffer::Create(
            quadIndices.get(),
            Renderer2DData::MaxIndices);
        s_Data->QuadVertexArray->SetIndexBuffer(quadIB);
        s_Data->WhiteTexture = Texture2D::Create(1, 1);
        std::uint32_t whiteTextureData = 0xffffffffu;
        s_Data->WhiteTexture->SetData(
            &whiteTextureData,
            sizeof(whiteTextureData));
        s_Data->TextureShader = Shader::Create("assets/shaders/Texture.glsl");
        s_Data->TextureShader->Bind();
        int samplers[Renderer2DData::MaxTextureSlots];
        for (std::uint32_t i = 0; i < Renderer2DData::MaxTextureSlots; i++)
        {
            samplers[i] = static_cast<int>(i);
        }
        s_Data->TextureShader->SetIntArray(
            "u_Textures",
            samplers,
            Renderer2DData::MaxTextureSlots);
        s_Data->TextureSlots[0] = s_Data->WhiteTexture;
        s_Data->QuadVertexPositions = {{
            {-0.5f, -0.5f, 0.0f, 1.0f},
            { 0.5f, -0.5f, 0.0f, 1.0f},
            { 0.5f,  0.5f, 0.0f, 1.0f},
            {-0.5f,  0.5f, 0.0f, 1.0f}
        }};
    }

    void Renderer2D::Shutdown()
    {
        HZ_PROFILE_FUNCTION();
        s_Data.reset();
        Font::Shutdown();
    }

    void Renderer2D::BeginScene(const OrthographicCamera& camera)
    {
        HZ_PROFILE_FUNCTION();
        s_Data->TextureShader->Bind();
        s_Data->TextureShader->SetMat4(
            "u_ViewProjection",
            camera.GetViewProjectionMatrix());
        s_Data->QuadIndexCount = 0;
        s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase.get();
        s_Data->TextureSlotIndex = 1;
    }

    void Renderer2D::EndScene()
    {
        HZ_PROFILE_FUNCTION();
        Flush();
    }

    void Renderer2D::Flush()
    {
        if (s_Data->QuadIndexCount == 0)
            return;
        const auto vertexCount =
            s_Data->QuadVertexBufferPtr -
            s_Data->QuadVertexBufferBase.get();
        const auto byteCount = static_cast<std::uint32_t>(vertexCount * sizeof(QuadVertex));
        s_Data->QuadVertexBuffer->SetData(s_Data->QuadVertexBufferBase.get(), byteCount);
        for (std::uint32_t i = 0; i < s_Data->TextureSlotIndex; i++)
        {
            s_Data->TextureSlots[i]->Bind(i);
        }
        s_Data->TextureShader->Bind();
        s_Data->QuadVertexArray->Bind();
        RenderCommand::DrawIndexed(s_Data->QuadVertexArray, s_Data->QuadIndexCount);
        s_Data->Stats.DrawCalls++;
    }

    void Renderer2D::FlushAndReset()
    {
        Flush();
        s_Data->QuadIndexCount = 0;
        s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase.get();
        s_Data->TextureSlotIndex = 1;
    }

    float Renderer2D::GetTextureIndex(const Ref<Texture2D>& texture)
    {
        HZ_CORE_ASSERT(texture, "Renderer2D received a null texture");
        for (std::uint32_t i = 1; i < s_Data->TextureSlotIndex; i++)
        {
            if (*s_Data->TextureSlots[i] == *texture)
                return static_cast<float>(i);
        }
        if (s_Data->TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
        {
            FlushAndReset();
        }
        const float index = static_cast<float>(s_Data->TextureSlotIndex);
        s_Data->TextureSlots[s_Data->TextureSlotIndex++] = texture;
        return index;
    }

    static void WriteQuad(
        const glm::mat4& transform,
        const glm::vec4& color,
        float textureIndex,
        float tilingFactor)
    {
        constexpr glm::vec2 textureCoords[4] = {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f}
        };
        for (std::size_t i = 0; i < 4; i++)
        {
            QuadVertex& vertex = *s_Data->QuadVertexBufferPtr++;
            vertex.Position = glm::vec3(transform * s_Data->QuadVertexPositions[i]);
            vertex.Color = color;
            vertex.TexCoord = textureCoords[i];
            vertex.TexIndex = textureIndex;
            vertex.TilingFactor = tilingFactor;
        }
        s_Data->QuadIndexCount += 6;
        s_Data->Stats.QuadCount++;
    }

    void Renderer2D::DrawQuad(
        const glm::vec2& position,
        const glm::vec2& size,
        const glm::vec4& color)
    {
        DrawQuad(
            {position.x, position.y, 0.0f},
            size,
            color);
    }

    void Renderer2D::DrawQuad(
        const glm::vec3& position,
        const glm::vec2& size,
        const glm::vec4& color)
    {
        HZ_PROFILE_FUNCTION();
        if (s_Data->QuadIndexCount >= Renderer2DData::MaxIndices)
        {
            FlushAndReset();
        }

        const glm::mat4 transform =
            glm::translate(
                glm::mat4(1.0f),
                position) *
            glm::scale(
                glm::mat4(1.0f),
                {size.x, size.y, 1.0f});

        WriteQuad(transform, color, 0.0f, 1.0f);
    }

    void Renderer2D::DrawQuad(
        const glm::vec2& position,
        const glm::vec2& size,
        const Ref<Texture2D>& texture,
        float tilingFactor,
        const glm::vec4& tintColor)
    {
        DrawQuad(
            {position.x, position.y, 0.0f},
            size,
            texture,
            tilingFactor,
            tintColor);
    }

    void Renderer2D::DrawQuad(
        const glm::vec3& position,
        const glm::vec2& size,
        const Ref<Texture2D>& texture,
        float tilingFactor,
        const glm::vec4& tintColor)
    {
        HZ_PROFILE_FUNCTION();
        if (s_Data->QuadIndexCount >= Renderer2DData::MaxIndices)
        {
            FlushAndReset();
        }

        const float textureIndex = GetTextureIndex(texture);

        const glm::mat4 transform =
            glm::translate(
                glm::mat4(1.0f),
                position) *
            glm::scale(
                glm::mat4(1.0f),
                {size.x, size.y, 1.0f});

        WriteQuad(
            transform,
            tintColor,
            textureIndex,
            tilingFactor);
    }

    void Renderer2D::DrawRotatedQuad(
        const glm::vec2& position,
        const glm::vec2& size,
        float rotation,
        const glm::vec4& color)
    {
        DrawRotatedQuad(
            {position.x, position.y, 0.0f},
            size,
            rotation,
            color);
    }

    void Renderer2D::DrawRotatedQuad(
        const glm::vec3& position,
        const glm::vec2& size,
        float rotation,
        const glm::vec4& color)
    {
        HZ_PROFILE_FUNCTION();
        if (s_Data->QuadIndexCount >= Renderer2DData::MaxIndices)
        {
            FlushAndReset();
        }

        const glm::mat4 transform =
            glm::translate(
                glm::mat4(1.0f),
                position) *
            glm::rotate(
                glm::mat4(1.0f),
                rotation,
                {0.0f, 0.0f, 1.0f}) *
            glm::scale(
                glm::mat4(1.0f),
                {size.x, size.y, 1.0f});

        WriteQuad(transform, color, 0.0f, 1.0f);
    }

    void Renderer2D::DrawRotatedQuad(
        const glm::vec2& position,
        const glm::vec2& size,
        float rotation,
        const Ref<Texture2D>& texture,
        float tilingFactor,
        const glm::vec4& tintColor)
    {
        DrawRotatedQuad(
            {position.x, position.y, 0.0f},
            size,
            rotation,
            texture,
            tilingFactor,
            tintColor);
    }

    void Renderer2D::DrawRotatedQuad(
        const glm::vec3& position,
        const glm::vec2& size,
        float rotation,
        const Ref<Texture2D>& texture,
        float tilingFactor,
        const glm::vec4& tintColor)
    {
        HZ_PROFILE_FUNCTION();

        if (s_Data->QuadIndexCount >= Renderer2DData::MaxIndices)
        {
            FlushAndReset();
        }

        const float textureIndex = GetTextureIndex(texture);

        const glm::mat4 transform =
            glm::translate(
                glm::mat4(1.0f),
                position) *
            glm::rotate(
                glm::mat4(1.0f),
                rotation,
                {0.0f, 0.0f, 1.0f}) *
            glm::scale(
                glm::mat4(1.0f),
                {size.x, size.y, 1.0f});

        WriteQuad(
            transform,
            tintColor,
            textureIndex,
            tilingFactor);
    }

    void Renderer2D::ResetStats()
    {
        s_Data->Stats = {};
    }

    Renderer2D::Statistics Renderer2D::GetStats()
    {
        return s_Data->Stats;
    }
}
