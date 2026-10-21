// Actual upstream Renderer2D adapted for owned state, complete capacity guards and backend limits.
#include "hzpch.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/Renderer2D.h"

#include "Hazel/Renderer/VertexArray.h"
#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/UniformBuffer.h"
#include "Hazel/Renderer/RenderCommand.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "MSDFData.h"
#include "Hazel/Utils/UTF8.h"
#include <algorithm>
#include <stdexcept>

namespace Hazel {

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
		float TilingFactor;

		// Editor-only
		int EntityID;
        glm::vec4 ClipRect;
	};

	struct CircleVertex
	{
		glm::vec3 WorldPosition;
		glm::vec3 LocalPosition;
		glm::vec4 Color;
		float Thickness;
		float Fade;

		// Editor-only
		int EntityID;
        glm::vec4 ClipRect;
	};

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;

		// Editor-only
		int EntityID;
        glm::vec4 ClipRect;
	};

	struct TextVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;

		// TODO: bg color for outline/bg

		// Editor-only
		int EntityID;
        glm::vec4 ClipRect;
	};

	struct Renderer2DData
	{
		static constexpr uint32_t MaxQuads = 20000;
		static constexpr uint32_t MaxVertices = MaxQuads * 4;
		static constexpr uint32_t MaxIndices = MaxQuads * 6;
		static constexpr uint32_t MaxTextureSlots = 32;
		uint32_t TextureSlotLimit = 0;
        bool Ordered=false;
        glm::vec4 ClipRect{-1};
        struct Run {int Kind;uint32_t First,Count;Ref<Texture2D> Font;float LineWidth;};
        std::vector<Run> Runs; // at most 8192, reused between chunks/frames


		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<Shader> QuadShader;
		Ref<Texture2D> WhiteTexture;

		Ref<VertexArray> CircleVertexArray;
		Ref<VertexBuffer> CircleVertexBuffer;
		Ref<Shader> CircleShader;

		Ref<VertexArray> LineVertexArray;
		Ref<VertexBuffer> LineVertexBuffer;
		Ref<Shader> LineShader;

		Ref<VertexArray> TextVertexArray;
		Ref<VertexBuffer> TextVertexBuffer;
		Ref<Shader> TextShader;

		uint32_t QuadIndexCount = 0;
		Scope<QuadVertex[]> QuadVertexBufferBase;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		uint32_t CircleIndexCount = 0;
		Scope<CircleVertex[]> CircleVertexBufferBase;
		CircleVertex* CircleVertexBufferPtr = nullptr;

		uint32_t LineVertexCount = 0;
		Scope<LineVertex[]> LineVertexBufferBase;
		LineVertex* LineVertexBufferPtr = nullptr;

		uint32_t TextIndexCount = 0;
		Scope<TextVertex[]> TextVertexBufferBase;
		TextVertex* TextVertexBufferPtr = nullptr;

		float LineWidth = 2.0f;

		std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1; // 0 = white texture

		Ref<Texture2D> FontAtlasTexture;
        uint32_t FontSamplerUnit=0;

		glm::vec4 QuadVertexPositions[4];

		Renderer2D::Statistics Stats;

		struct CameraData
		{
			glm::mat4 ViewProjection;
		};
		CameraData CameraBuffer;
		Ref<UniformBuffer> CameraUniformBuffer;
	};

	static Scope<Renderer2DData> s_Data;

	void Renderer2D::Init()
	{
		HZ_PROFILE_FUNCTION();

		Font::Initialize();
        s_Data = CreateScope<Renderer2DData>();
		s_Data->TextureSlotLimit = RenderCommand::GetSettings().TextureSlots;
		HZ_CORE_INFO("Renderer2D texture slots: {}", s_Data->TextureSlotLimit);
		s_Data->QuadVertexArray = VertexArray::Create();

		s_Data->QuadVertexBuffer = VertexBuffer::Create(s_Data->MaxVertices * sizeof(QuadVertex));
		s_Data->QuadVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position"     },
			{ ShaderDataType::Float4, "a_Color"        },
			{ ShaderDataType::Float2, "a_TexCoord"     },
			{ ShaderDataType::Float,  "a_TexIndex"     },
			{ ShaderDataType::Float,  "a_TilingFactor" },
			{ ShaderDataType::Int, "a_EntityID" },
            { ShaderDataType::Float4, "a_ClipRect" }
		});
		s_Data->QuadVertexArray->AddVertexBuffer(s_Data->QuadVertexBuffer);

		s_Data->QuadVertexBufferBase = CreateScope<QuadVertex[]>(s_Data->MaxVertices);

		auto quadIndices = CreateScope<uint32_t[]>(s_Data->MaxIndices);

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_Data->MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		Ref<IndexBuffer> quadIB = IndexBuffer::Create(quadIndices.get(), s_Data->MaxIndices);
		s_Data->QuadVertexArray->SetIndexBuffer(quadIB);
		quadIndices.reset();

		// Circles
		s_Data->CircleVertexArray = VertexArray::Create();

		s_Data->CircleVertexBuffer = VertexBuffer::Create(s_Data->MaxVertices * sizeof(CircleVertex));
		s_Data->CircleVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_WorldPosition" },
			{ ShaderDataType::Float3, "a_LocalPosition" },
			{ ShaderDataType::Float4, "a_Color"         },
			{ ShaderDataType::Float,  "a_Thickness"     },
			{ ShaderDataType::Float,  "a_Fade"          },
			{ ShaderDataType::Int, "a_EntityID" },
            { ShaderDataType::Float4, "a_ClipRect" }
		});
		s_Data->CircleVertexArray->AddVertexBuffer(s_Data->CircleVertexBuffer);
		s_Data->CircleVertexArray->SetIndexBuffer(quadIB); // Use quad IB
		s_Data->CircleVertexBufferBase = CreateScope<CircleVertex[]>(s_Data->MaxVertices);

		// Lines
		s_Data->LineVertexArray = VertexArray::Create();

		s_Data->LineVertexBuffer = VertexBuffer::Create(s_Data->MaxVertices * sizeof(LineVertex));
		s_Data->LineVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color"    },
			{ ShaderDataType::Int, "a_EntityID" },
            { ShaderDataType::Float4, "a_ClipRect" }
		});
		s_Data->LineVertexArray->AddVertexBuffer(s_Data->LineVertexBuffer);
		s_Data->LineVertexBufferBase = CreateScope<LineVertex[]>(s_Data->MaxVertices);

		// Text
		s_Data->TextVertexArray = VertexArray::Create();

		s_Data->TextVertexBuffer = VertexBuffer::Create(s_Data->MaxVertices * sizeof(TextVertex));
		s_Data->TextVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position"     },
			{ ShaderDataType::Float4, "a_Color"        },
			{ ShaderDataType::Float2, "a_TexCoord"     },
			{ ShaderDataType::Int, "a_EntityID" },
            { ShaderDataType::Float4, "a_ClipRect" }
		});
		s_Data->TextVertexArray->AddVertexBuffer(s_Data->TextVertexBuffer);
		s_Data->TextVertexArray->SetIndexBuffer(quadIB);
		s_Data->TextVertexBufferBase = CreateScope<TextVertex[]>(s_Data->MaxVertices);

		s_Data->WhiteTexture = Texture2D::Create(TextureSpecification());
		uint32_t whiteTextureData = 0xffffffff;
		s_Data->WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		s_Data->QuadShader = Shader::Create(Resources::Resolve("shaders/Renderer2D_Quad.glsl").generic_u8string());
		s_Data->CircleShader = Shader::Create(Resources::Resolve("shaders/Renderer2D_Circle.glsl").generic_u8string());
		s_Data->LineShader = Shader::Create(Resources::Resolve("shaders/Renderer2D_Line.glsl").generic_u8string());
		s_Data->TextShader = Shader::Create(Resources::Resolve("shaders/Renderer2D_Text.glsl").generic_u8string());

		// Set first texture slot to 0
		s_Data->TextureSlots[0] = s_Data->WhiteTexture;

		s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		s_Data->CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer2DData::CameraData), 0);
        Font::GetDefault(); // reserve the context-owned fallback before any custom admission
	}

	void Renderer2D::Shutdown()
	{
		HZ_PROFILE_FUNCTION();

		s_Data.reset();
		Font::Shutdown();
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
        s_Data->Ordered=false;s_Data->ClipRect=glm::vec4(-1);
		HZ_PROFILE_FUNCTION();

		s_Data->CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data->CameraUniformBuffer->SetData(&s_Data->CameraBuffer, sizeof(Renderer2DData::CameraData));

		StartBatch();
	}

	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
        s_Data->Ordered=false;s_Data->ClipRect=glm::vec4(-1);
		HZ_PROFILE_FUNCTION();

		s_Data->CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data->CameraUniformBuffer->SetData(&s_Data->CameraBuffer, sizeof(Renderer2DData::CameraData));

		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& camera)
	{
        s_Data->Ordered=false;s_Data->ClipRect=glm::vec4(-1);
		HZ_PROFILE_FUNCTION();

		s_Data->CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data->CameraUniformBuffer->SetData(&s_Data->CameraBuffer, sizeof(Renderer2DData::CameraData));

		StartBatch();
	}

    void Renderer2D::BeginOrderedScene(const Camera &camera, const glm::mat4 &transform)
    {
        // Drain the previous pass before updating its camera or raster state.
        EndScene();
        BeginScene(camera, transform);
        s_Data->Ordered = true;
    }
    void Renderer2D::SetPrimitiveClip(glm::ivec4 clip)
    {
        if (!s_Data->Ordered)
            throw std::logic_error("Primitive clip requires an ordered pass");
        if (s_Data->ClipRect != glm::vec4(clip))
            ++s_Data->Stats.ClipTransitions;
        s_Data->ClipRect = glm::vec4(clip);
    }
    void Renderer2D::EndScene()
    {
        HZ_PROFILE_FUNCTION();
        Flush(FlushReason::End);
        StartBatch();
        if (s_Data->FontSamplerUnit)
        {
            s_Data->TextShader->Bind();
            s_Data->TextShader->SetInt("u_FontAtlas", 0);
            s_Data->FontSamplerUnit = 0;
        }
        s_Data->Ordered = false;
        s_Data->ClipRect = glm::vec4(-1);
        s_Data->FontAtlasTexture.reset();
    }
    void Renderer2D::EnsureRunSpace()
    {
        if (s_Data->Ordered && s_Data->Runs.size() >= 8192)
            NextBatch(FlushReason::Capacity);
    }
    void Renderer2D::QueueRun(Primitive kind, uint32_t count)
    {
        if (!s_Data->Ordered)
            return;
        const int type = int(kind);
        auto &runs = s_Data->Runs;
        if (!runs.empty())
        {
            auto &last = runs.back();
            if (last.Kind == type &&
                (kind != Primitive::Text || last.Font == s_Data->FontAtlasTexture) &&
                (kind != Primitive::Line || last.LineWidth == s_Data->LineWidth))
            {
                last.Count += count;
                return;
            }
            if (last.Kind != type)
                ++s_Data->Stats.PrimitiveTransitions;
            else if (kind == Primitive::Text)
                ++s_Data->Stats.FontTransitions;
        }
        uint32_t first = kind == Primitive::Quad     ? s_Data->QuadIndexCount
                         : kind == Primitive::Text   ? s_Data->TextIndexCount
                         : kind == Primitive::Circle ? s_Data->CircleIndexCount
                                                     : s_Data->LineVertexCount;
        runs.push_back({type, first, count,
                        kind == Primitive::Text ? s_Data->FontAtlasTexture : Ref<Texture2D>{},
                        s_Data->LineWidth});
    }
    void Renderer2D::FlushOrdered()
    {
        auto upload = [](auto &buffer, auto &base, auto end) {
            uint32_t bytes = uint32_t(reinterpret_cast<uint8_t *>(end) -
                                      reinterpret_cast<uint8_t *>(base.get()));
            if (bytes)
            {
                buffer->SetData(base.get(), bytes);
                s_Data->Stats.UploadedBytes += bytes;
            }
        };
        upload(s_Data->QuadVertexBuffer, s_Data->QuadVertexBufferBase, s_Data->QuadVertexBufferPtr);
        upload(s_Data->CircleVertexBuffer, s_Data->CircleVertexBufferBase,
               s_Data->CircleVertexBufferPtr);
        upload(s_Data->LineVertexBuffer, s_Data->LineVertexBufferBase, s_Data->LineVertexBufferPtr);
        upload(s_Data->TextVertexBuffer, s_Data->TextVertexBufferBase, s_Data->TextVertexBufferPtr);
        for (uint32_t i = 0; s_Data->QuadIndexCount && i < s_Data->TextureSlotIndex; ++i)
            s_Data->TextureSlots[i]->Bind(i);
        // Geometry is now complete: use an actually free sampler for text when
        // possible, preserving every configured quad slot. A full chunk shares
        // unit zero and restores white before a subsequent quad run.
        // SPIR-V does not guarantee named uniform lookup. Its declared sampler
        // binding stays immutable; use the same shared-unit fallback as full chunks.
        const bool movableSampler =
            s_Data->TextShader->GetProgramLoadingPath() != Shader::ProgramLoadingPath::SPIRV;
        const uint32_t fontUnit = movableSampler && s_Data->QuadIndexCount &&
                                          s_Data->TextureSlotIndex < s_Data->TextureSlotLimit
                                      ? s_Data->TextureSlotIndex
                                      : 0;
        if (s_Data->TextIndexCount && s_Data->FontSamplerUnit != fontUnit)
        {
            s_Data->TextShader->Bind();
            s_Data->TextShader->SetInt("u_FontAtlas", fontUnit);
            s_Data->FontSamplerUnit = fontUnit;
        }
        Ref<Texture2D> unitZero = s_Data->QuadIndexCount ? s_Data->WhiteTexture : Ref<Texture2D>{},
                       boundFont;
        for (const auto &run : s_Data->Runs)
        {
            RenderCommand::SetTextCompositing(Primitive(run.Kind) == Primitive::Text);
            switch (Primitive(run.Kind))
            {
            case Primitive::Quad:
                if (unitZero != s_Data->WhiteTexture)
                {
                    s_Data->WhiteTexture->Bind(0);
                    unitZero = s_Data->WhiteTexture;
                }
                s_Data->QuadShader->Bind();
                RenderCommand::DrawIndexed(s_Data->QuadVertexArray, run.Count, run.First);
                break;
            case Primitive::Text:
                if ((fontUnit ? boundFont : unitZero) != run.Font)
                {
                    run.Font->Bind(fontUnit);
                    if (fontUnit)
                        boundFont = run.Font;
                    else
                        unitZero = run.Font;
                }
                s_Data->TextShader->Bind();
                RenderCommand::DrawIndexed(s_Data->TextVertexArray, run.Count, run.First);
                break;
            case Primitive::Circle:
                s_Data->CircleShader->Bind();
                RenderCommand::DrawIndexed(s_Data->CircleVertexArray, run.Count, run.First);
                break;
            case Primitive::Line:
                s_Data->LineShader->Bind();
                RenderCommand::SetLineWidth(run.LineWidth);
                RenderCommand::DrawLines(s_Data->LineVertexArray, run.Count, run.First);
                break;
            }
            ++s_Data->Stats.DrawCalls;
        }
    }

    void Renderer2D::StartBatch()
	{
		s_Data->QuadIndexCount = 0;
		s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase.get();

		s_Data->CircleIndexCount = 0;
		s_Data->CircleVertexBufferPtr = s_Data->CircleVertexBufferBase.get();

		s_Data->LineVertexCount = 0;
		s_Data->LineVertexBufferPtr = s_Data->LineVertexBufferBase.get();

		s_Data->TextIndexCount = 0;
		s_Data->TextVertexBufferPtr = s_Data->TextVertexBufferBase.get();

		for(uint32_t i=1;i<s_Data->TextureSlotIndex;++i)s_Data->TextureSlots[i].reset();
        s_Data->TextureSlotIndex = 1;
        s_Data->Runs.clear();
	}

	void Renderer2D::Flush(FlushReason reason)
	{
        if(!(s_Data->QuadIndexCount||s_Data->CircleIndexCount||s_Data->TextIndexCount||s_Data->LineVertexCount))return;
        const auto start=std::chrono::steady_clock::now();
        struct TextCompositingScope {
            bool Active;
            explicit TextCompositingScope(bool active):Active(active){if(Active)RenderCommand::BeginTextCompositing();}
            void Finish(){if(Active){RenderCommand::EndTextCompositing();Active=false;}}
            ~TextCompositingScope(){Finish();}
        } textScope(s_Data->TextIndexCount!=0);
        if(s_Data->QuadIndexCount||s_Data->CircleIndexCount||s_Data->TextIndexCount||s_Data->LineVertexCount)++s_Data->Stats.Flushes[size_t(reason)];
        if(s_Data->Ordered){
            FlushOrdered();textScope.Finish();s_Data->Stats.SubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return;
        }
		if (s_Data->QuadIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data->QuadVertexBufferPtr - (uint8_t*)s_Data->QuadVertexBufferBase.get());
			s_Data->QuadVertexBuffer->SetData(s_Data->QuadVertexBufferBase.get(), dataSize);
            s_Data->Stats.UploadedBytes+=dataSize;

			// Bind textures
			for (uint32_t i = 0; i < s_Data->TextureSlotIndex; i++)
				s_Data->TextureSlots[i]->Bind(i);

			s_Data->QuadShader->Bind();
			RenderCommand::DrawIndexed(s_Data->QuadVertexArray, s_Data->QuadIndexCount);
			s_Data->Stats.DrawCalls++;
		}

		if (s_Data->CircleIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data->CircleVertexBufferPtr - (uint8_t*)s_Data->CircleVertexBufferBase.get());
			s_Data->CircleVertexBuffer->SetData(s_Data->CircleVertexBufferBase.get(), dataSize);
            s_Data->Stats.UploadedBytes+=dataSize;

			s_Data->CircleShader->Bind();
			RenderCommand::DrawIndexed(s_Data->CircleVertexArray, s_Data->CircleIndexCount);
			s_Data->Stats.DrawCalls++;
		}

		if (s_Data->LineVertexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data->LineVertexBufferPtr - (uint8_t*)s_Data->LineVertexBufferBase.get());
			s_Data->LineVertexBuffer->SetData(s_Data->LineVertexBufferBase.get(), dataSize);
            s_Data->Stats.UploadedBytes+=dataSize;

			s_Data->LineShader->Bind();
			RenderCommand::SetLineWidth(s_Data->LineWidth);
			RenderCommand::DrawLines(s_Data->LineVertexArray, s_Data->LineVertexCount);
			s_Data->Stats.DrawCalls++;
		}

		if (s_Data->TextIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data->TextVertexBufferPtr - (uint8_t*)s_Data->TextVertexBufferBase.get());
			s_Data->TextVertexBuffer->SetData(s_Data->TextVertexBufferBase.get(), dataSize);
            s_Data->Stats.UploadedBytes+=dataSize;

			s_Data->FontAtlasTexture->Bind(0);

			s_Data->TextShader->Bind();
            RenderCommand::SetTextCompositing(true);
			RenderCommand::DrawIndexed(s_Data->TextVertexArray, s_Data->TextIndexCount);
			s_Data->Stats.DrawCalls++;
		}
        textScope.Finish();
        s_Data->Stats.SubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
	}

	void Renderer2D::NextBatch(FlushReason reason)
	{
		Flush(reason);
		StartBatch();
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		HZ_PROFILE_FUNCTION();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, color);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		HZ_PROFILE_FUNCTION();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
        EnsureRunSpace();
		HZ_PROFILE_FUNCTION();

		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		const float tilingFactor = 1.0f;

		if (s_Data->QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[i];
			s_Data->QuadVertexBufferPtr->Color = color;
			s_Data->QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data->QuadVertexBufferPtr->EntityID = entityID;
            s_Data->QuadVertexBufferPtr->ClipRect=s_Data->ClipRect;
			s_Data->QuadVertexBufferPtr++;
		}

		QueueRun(Primitive::Quad,6);
        s_Data->QuadIndexCount += 6;

		s_Data->Stats.QuadCount++;
        ++s_Data->Stats.GeometryCount;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID)
	{
        ResolvedSprite sprite; sprite.Texture=texture; sprite.TilingFactor=tilingFactor;
        DrawSprite(transform,sprite,tintColor,entityID);
    }

    void Renderer2D::DrawSprite(const glm::mat4& transform, const ResolvedSprite& sprite, const glm::vec4& tintColor, int entityID)
	{
        EnsureRunSpace();
		HZ_PROFILE_FUNCTION();
        const auto& texture=sprite.Texture;

		constexpr size_t quadVertexCount = 4;

		if (s_Data->QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; texture && i < s_Data->TextureSlotIndex; i++)
		{
			if (*s_Data->TextureSlots[i] == *texture)
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (texture && textureIndex == 0.0f)
		{
			if (s_Data->TextureSlotIndex >= s_Data->TextureSlotLimit)
				NextBatch(FlushReason::Textures);

			textureIndex = (float)s_Data->TextureSlotIndex;
			s_Data->TextureSlots[s_Data->TextureSlotIndex] = texture;
			s_Data->TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data->QuadVertexBufferPtr->Position = transform * glm::vec4(sprite.Corners[i],0,1);
			s_Data->QuadVertexBufferPtr->Color = tintColor;
			s_Data->QuadVertexBufferPtr->TexCoord = sprite.UV[i];
			s_Data->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data->QuadVertexBufferPtr->TilingFactor = sprite.TilingFactor;
			s_Data->QuadVertexBufferPtr->EntityID = entityID;
            s_Data->QuadVertexBufferPtr->ClipRect=s_Data->ClipRect;
			s_Data->QuadVertexBufferPtr++;
		}

		QueueRun(Primitive::Quad,6);
        s_Data->QuadIndexCount += 6;

		s_Data->Stats.QuadCount++;
        ++s_Data->Stats.GeometryCount;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		HZ_PROFILE_FUNCTION();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		HZ_PROFILE_FUNCTION();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness /*= 1.0f*/, float fade /*= 0.005f*/, int entityID /*= -1*/)
	{
        EnsureRunSpace();
		HZ_PROFILE_FUNCTION();

		if (s_Data->CircleIndexCount + 6 > Renderer2DData::MaxIndices)
			NextBatch();

		for (size_t i = 0; i < 4; i++)
		{
			s_Data->CircleVertexBufferPtr->WorldPosition = transform * s_Data->QuadVertexPositions[i];
			s_Data->CircleVertexBufferPtr->LocalPosition = s_Data->QuadVertexPositions[i] * 2.0f;
			s_Data->CircleVertexBufferPtr->Color = color;
			s_Data->CircleVertexBufferPtr->Thickness = thickness;
			s_Data->CircleVertexBufferPtr->Fade = fade;
			s_Data->CircleVertexBufferPtr->EntityID = entityID;
            s_Data->CircleVertexBufferPtr->ClipRect=s_Data->ClipRect;
			s_Data->CircleVertexBufferPtr++;
		}

		QueueRun(Primitive::Circle,6);
        s_Data->CircleIndexCount += 6;

		s_Data->Stats.QuadCount++;
        ++s_Data->Stats.GeometryCount;
	}

	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID)
	{
        EnsureRunSpace();
		if (s_Data->LineVertexCount + 2 > Renderer2DData::MaxVertices)
			NextBatch();
		s_Data->LineVertexBufferPtr->Position = p0;
		s_Data->LineVertexBufferPtr->Color = color;
		s_Data->LineVertexBufferPtr->EntityID = entityID;
            s_Data->LineVertexBufferPtr->ClipRect=s_Data->ClipRect;
		s_Data->LineVertexBufferPtr++;

		s_Data->LineVertexBufferPtr->Position = p1;
		s_Data->LineVertexBufferPtr->Color = color;
		s_Data->LineVertexBufferPtr->EntityID = entityID;
            s_Data->LineVertexBufferPtr->ClipRect=s_Data->ClipRect;
		s_Data->LineVertexBufferPtr++;

		QueueRun(Primitive::Line,2);
        s_Data->LineVertexCount += 2;
        ++s_Data->Stats.GeometryCount;
	}

	void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID)
	{
		glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
		glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

		DrawLine(p0, p1, color, entityID);
		DrawLine(p1, p2, color, entityID);
		DrawLine(p2, p3, color, entityID);
		DrawLine(p3, p0, color, entityID);
	}

	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
		glm::vec3 lineVertices[4];
		for (size_t i = 0; i < 4; i++)
			lineVertices[i] = transform * s_Data->QuadVertexPositions[i];

		DrawLine(lineVertices[0], lineVertices[1], color, entityID);
		DrawLine(lineVertices[1], lineVertices[2], color, entityID);
		DrawLine(lineVertices[2], lineVertices[3], color, entityID);
		DrawLine(lineVertices[3], lineVertices[0], color, entityID);
	}

	void Renderer2D::DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID)
	{
		if (src.Resolved.Data)
			DrawSprite(transform, *src.Resolved.Data, src.Color, entityID);
		else
			DrawQuad(transform, src.Color, entityID);
	}


    float Renderer2D::MeasureText(const std::string& text,const Ref<Font>& font,float kerning) {
        if(!font)throw std::invalid_argument("Measurement needs a font");
        const auto& g=font->GetMSDFData()->FontGeometry;const auto& metrics=g.getMetrics();
        const double scale=1/(metrics.ascenderY-metrics.descenderY);
        thread_local std::vector<uint32_t> chars;UTF8::Decode(text,chars);
        double x=0,maximum=0;
        for(size_t i=0;i<chars.size();i++) {
            auto ch=chars[i];if(ch=='\r'||ch==0xfeff)continue;if(ch=='\n'){maximum=std::max(maximum,x);x=0;continue;}
            if(ch=='\t'){x+=4*(scale*g.getGlyph(' ')->getAdvance()+kerning);continue;}
            auto glyph=g.getGlyph(ch);if(!glyph)glyph=g.getGlyph('?');if(!glyph)continue;
            double advance=glyph->getAdvance();if(i+1<chars.size())g.getAdvance(advance,ch,chars[i+1]);x+=scale*advance+kerning;
        }
        return float(std::max(maximum,x));
    }
    void Renderer2D::LayoutText(const std::string& text,const Ref<Font>& font,float width,bool wrap,float kerning,std::vector<TextLine>& lines) {
        if(text.size()>65536)throw std::invalid_argument("UI text exceeds 64 KiB");
        lines.clear();
        if(!font)throw std::invalid_argument("Layout needs a font");
        const auto& geometry=font->GetMSDFData()->FontGeometry;const auto metrics=geometry.getMetrics();
        const double scale=1/(metrics.ascenderY-metrics.descenderY);
        thread_local std::vector<uint32_t> chars;
        thread_local std::vector<size_t> bytes;UTF8::Decode(text,chars,&bytes);
        auto base=[&](uint32_t ch){auto glyph=geometry.getGlyph(ch);if(!glyph)glyph=geometry.getGlyph('?');return glyph?glyph->getAdvance():0.;};
        auto emit=[&](size_t begin,size_t end){auto line=text.substr(bytes[begin],bytes[end]-bytes[begin]);lines.push_back({line,MeasureText(line,font,kerning)});};
        size_t start=0,lastSpace=std::string::npos;double advance=0,previous=0;uint32_t previousChar=0;
        for(size_t i=0;i<chars.size();i++) {
            const auto ch=chars[i];if(ch=='\r')continue;if(ch==0xfeff){previousChar=0;continue;}
            if(ch=='\n'){emit(start,i);start=i+1;advance=0;lastSpace=std::string::npos;previousChar=0;continue;}
            if(previousChar&&previousChar!='\t'){double paired=base(previousChar);geometry.getAdvance(paired,previousChar,ch);advance+=scale*paired+kerning-previous;}
            double current=ch=='\t'?4*(scale*base(' ')+kerning):scale*base(ch)+kerning;
            if(wrap&&width>0&&i>start&&advance+current>width) {
                size_t end=lastSpace!=std::string::npos&&lastSpace>=start?lastSpace:i;
                emit(start,end);start=end;
                while(start<chars.size()&&(chars[start]==' '||chars[start]=='\t'))++start;
                advance=0;previousChar=0;lastSpace=std::string::npos;i=start-1;continue;
            }
            advance+=current;previous=current;previousChar=ch;if(ch==' '||ch=='\t')lastSpace=i;
        }
        emit(start,chars.size());
    }
    void Renderer2D::AppendTextGlyphs(const std::string &string, const Ref<Font> &font,
                                      const TextParams &params, std::vector<TextGlyph> &glyphs,
                                      glm::vec2 offset)
    {
        if (string.empty())
            return;
        thread_local std::vector<uint32_t> codepoints;
        UTF8::Decode(string, codepoints);
        if (!font || !font->GetAtlasTexture())
            throw std::invalid_argument("Text requires a usable font");
        const auto &geometry = font->GetMSDFData()->FontGeometry;
        const auto &metrics = geometry.getMetrics();
        const double scale = 1.0 / (metrics.ascenderY - metrics.descenderY);
        double x = 0, y = 0;
        const float space = geometry.getGlyph(' ')->getAdvance();
        const glm::vec2 texel(1.f / font->GetAtlasTexture()->GetWidth(),
                              1.f / font->GetAtlasTexture()->GetHeight());
        for (size_t i = 0; i < codepoints.size(); ++i)
        {
            auto ch = codepoints[i];
            if (ch == '\r' || ch == 0xfeff)
                continue;
            if (ch == '\n')
            {
                x = 0;
                y -= scale * metrics.lineHeight + params.LineSpacing;
                continue;
            }
            if (ch == ' ')
            {
                float advance = space;
                if (i + 1 < codepoints.size())
                {
                    double paired = advance;
                    geometry.getAdvance(paired, ch, codepoints[i + 1]);
                    advance = float(paired);
                }
                x += scale * advance + params.Kerning;
                continue;
            }
            if (ch == '\t')
            {
                x += 4.f * (scale * space + params.Kerning);
                continue;
            }
            auto glyph = geometry.getGlyph(ch);
            if (!glyph)
                glyph = geometry.getGlyph('?');
            if (!glyph)
                return;
            double al, ab, ar, at, pl, pb, pr, pt;
            glyph->getQuadAtlasBounds(al, ab, ar, at);
            glyph->getQuadPlaneBounds(pl, pb, pr, pt);
            TextGlyph prepared;
            prepared.Min = glm::vec2(pl, pb) * float(scale) + glm::vec2(x, y) + offset;
            prepared.Max = glm::vec2(pr, pt) * float(scale) + glm::vec2(x, y) + offset;
            prepared.UVMin = glm::vec2(al, ab) * texel;
            prepared.UVMax = glm::vec2(ar, at) * texel;
            glyphs.push_back(prepared);
            if (i + 1 < codepoints.size())
            {
                double advance = glyph->getAdvance();
                geometry.getAdvance(advance, ch, codepoints[i + 1]);
                x += scale * advance + params.Kerning;
            }
        }
    }
    void Renderer2D::DrawTextGlyphs(const std::vector<TextGlyph> &glyphs, const Ref<Font> &font,
                                    const glm::mat4 &transform, const glm::vec4 &color,
                                    int entityID)
    {
        if (glyphs.empty())
            return;
        if (!font || !font->GetAtlasTexture())
            throw std::invalid_argument("Text requires a usable font");
        auto atlas = font->GetAtlasTexture();
        if (!s_Data->Ordered && s_Data->TextIndexCount && s_Data->FontAtlasTexture != atlas)
            NextBatch(FlushReason::Font);
        s_Data->FontAtlasTexture = atlas;
        for (const auto &g : glyphs)
        {
            EnsureRunSpace();
            if (s_Data->TextIndexCount + 6 > Renderer2DData::MaxIndices)
                NextBatch();
            const glm::vec2 positions[]{g.Min, {g.Min.x, g.Max.y}, g.Max, {g.Max.x, g.Min.y}};
            const glm::vec2 uv[]{g.UVMin, {g.UVMin.x, g.UVMax.y}, g.UVMax, {g.UVMax.x, g.UVMin.y}};
            for (size_t i = 0; i < 4; ++i)
            {
                auto &v = *s_Data->TextVertexBufferPtr++;
                v.Position = transform * glm::vec4(positions[i], 0, 1);
                v.Color = color;
                v.TexCoord = uv[i];
                v.EntityID = entityID;
                v.ClipRect = s_Data->ClipRect;
            }
            QueueRun(Primitive::Text, 6);
            s_Data->TextIndexCount += 6;
            ++s_Data->Stats.QuadCount;
            ++s_Data->Stats.GeometryCount;
        }
    }
    void Renderer2D::DrawString(const std::string &string, Ref<Font> font,
                                const glm::mat4 &transform, const TextParams &params, int entityID)
    {
        thread_local std::vector<TextGlyph> glyphs;
        glyphs.clear();
        AppendTextGlyphs(string, font, params, glyphs);
        DrawTextGlyphs(glyphs, font, transform, params.Color, entityID);
    }

    void Renderer2D::DrawString(const std::string& string, const glm::mat4& transform, const TextComponent& component, int entityID)
	{
		DrawString(string, component.FontAsset, transform, { component.Color, component.Kerning, component.LineSpacing }, entityID);
	}

	float Renderer2D::GetLineWidth()
	{
		return s_Data->LineWidth;
	}

	void Renderer2D::SetLineWidth(float width)
	{
		if (!(width > 0)) throw std::invalid_argument("Line width must be positive");
		s_Data->LineWidth = width;
	}

	uint32_t Renderer2D::GetGeometryCount() {return s_Data->Stats.GeometryCount;}

	void Renderer2D::ResetStats()
	{
		s_Data->Stats = {};
	}

	Shader::ProgramLoadingPath Renderer2D::GetQuadShaderLoadingPath() { if(!s_Data || !s_Data->QuadShader)throw std::logic_error("Renderer2D shaders are not initialized"); return s_Data->QuadShader->GetProgramLoadingPath(); }
	Renderer2D::Statistics Renderer2D::GetStats()
	{
		return s_Data->Stats;
	}

}
