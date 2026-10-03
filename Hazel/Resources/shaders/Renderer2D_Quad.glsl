// Basic Texture Shader

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in int a_EntityID;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
	float TilingFactor;
};

layout (location = 0) out VertexOutput Output;
layout (location = 3) out flat float v_TexIndex;
layout (location = 4) out flat int v_EntityID;

void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoord;
	Output.TilingFactor = a_TilingFactor;
	v_TexIndex = a_TexIndex;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
	float TilingFactor;
};

layout (location = 0) in VertexOutput Input;
layout (location = 3) in flat float v_TexIndex;
layout (location = 4) in flat int v_EntityID;

#ifndef HZ_MAX_TEXTURE_SLOTS
#define HZ_MAX_TEXTURE_SLOTS 32
#endif
layout (binding = 0) uniform sampler2D u_Textures[HZ_MAX_TEXTURE_SLOTS];

void main()
{
	vec4 texColor = Input.Color;

	switch(int(v_TexIndex))
	{
#if HZ_MAX_TEXTURE_SLOTS > 0
		case  0: texColor *= texture(u_Textures[ 0], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 1
		case  1: texColor *= texture(u_Textures[ 1], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 2
		case  2: texColor *= texture(u_Textures[ 2], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 3
		case  3: texColor *= texture(u_Textures[ 3], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 4
		case  4: texColor *= texture(u_Textures[ 4], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 5
		case  5: texColor *= texture(u_Textures[ 5], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 6
		case  6: texColor *= texture(u_Textures[ 6], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 7
		case  7: texColor *= texture(u_Textures[ 7], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 8
		case  8: texColor *= texture(u_Textures[ 8], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 9
		case  9: texColor *= texture(u_Textures[ 9], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 10
		case 10: texColor *= texture(u_Textures[10], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 11
		case 11: texColor *= texture(u_Textures[11], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 12
		case 12: texColor *= texture(u_Textures[12], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 13
		case 13: texColor *= texture(u_Textures[13], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 14
		case 14: texColor *= texture(u_Textures[14], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 15
		case 15: texColor *= texture(u_Textures[15], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 16
		case 16: texColor *= texture(u_Textures[16], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 17
		case 17: texColor *= texture(u_Textures[17], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 18
		case 18: texColor *= texture(u_Textures[18], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 19
		case 19: texColor *= texture(u_Textures[19], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 20
		case 20: texColor *= texture(u_Textures[20], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 21
		case 21: texColor *= texture(u_Textures[21], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 22
		case 22: texColor *= texture(u_Textures[22], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 23
		case 23: texColor *= texture(u_Textures[23], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 24
		case 24: texColor *= texture(u_Textures[24], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 25
		case 25: texColor *= texture(u_Textures[25], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 26
		case 26: texColor *= texture(u_Textures[26], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 27
		case 27: texColor *= texture(u_Textures[27], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 28
		case 28: texColor *= texture(u_Textures[28], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 29
		case 29: texColor *= texture(u_Textures[29], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 30
		case 30: texColor *= texture(u_Textures[30], Input.TexCoord * Input.TilingFactor); break;
#endif
#if HZ_MAX_TEXTURE_SLOTS > 31
		case 31: texColor *= texture(u_Textures[31], Input.TexCoord * Input.TilingFactor); break;
#endif
	}

	if (texColor.a == 0.0)
		discard;

	o_Color = texColor;
	o_EntityID = v_EntityID;
}
