//--------------------------
// - Hazel 2D -
// Renderer2D Line Shader
// --------------------------

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in int a_EntityID;

layout(location = 3) in vec4 a_ClipRect;
layout(location = 2) out flat vec4 v_ClipRect;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec4 Color;
};

layout (location = 0) out VertexOutput Output;
layout (location = 1) out flat int v_EntityID;

void main()
{
	Output.Color = a_Color;
	v_EntityID = a_EntityID;
    v_ClipRect = a_ClipRect;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 2) in flat vec4 v_ClipRect;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput
{
	vec4 Color;
};

layout (location = 0) in VertexOutput Input;
layout (location = 1) in flat int v_EntityID;

void main()
{
	o_Color = Input.Color;
	// Evaluate texture derivatives/coverage before the nonuniform clip discard.
    if (v_ClipRect.z >= 0.0 && (gl_FragCoord.x < v_ClipRect.x || gl_FragCoord.y < v_ClipRect.y ||
        gl_FragCoord.x >= v_ClipRect.x + v_ClipRect.z || gl_FragCoord.y >= v_ClipRect.y + v_ClipRect.w)) discard;
    o_EntityID = v_EntityID;
}
