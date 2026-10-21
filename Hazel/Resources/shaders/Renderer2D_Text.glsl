// MSDF text shader

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in int a_EntityID;

layout(location = 4) in vec4 a_ClipRect;
layout(location = 3) out flat vec4 v_ClipRect;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
};

layout (location = 0) out VertexOutput Output;
layout (location = 2) out flat int v_EntityID;

void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoord;
	v_EntityID = a_EntityID;
    v_ClipRect = a_ClipRect;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 3) in flat vec4 v_ClipRect;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
};

layout (location = 0) in VertexOutput Input;
layout (location = 2) in flat int v_EntityID;

layout (binding = 0) uniform sampler2D u_FontAtlas;

float screenPxRange() {
	const float pxRange = 2.0; // set to distance field's pixel range
    vec2 unitRange = vec2(pxRange)/vec2(textureSize(u_FontAtlas, 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(Input.TexCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
	vec3 msd = texture(u_FontAtlas, Input.TexCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange()*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
	if (opacity == 0.0)
		discard;

    // Straight-alpha source: coverage belongs only in alpha. RGB is the tint.
    o_Color = vec4(Input.Color.rgb, Input.Color.a * opacity);
	if (o_Color.a == 0.0)
		discard;

	// Evaluate texture derivatives/coverage before the nonuniform clip discard.
    if (v_ClipRect.z >= 0.0 && (gl_FragCoord.x < v_ClipRect.x || gl_FragCoord.y < v_ClipRect.y ||
        gl_FragCoord.x >= v_ClipRect.x + v_ClipRect.z || gl_FragCoord.y >= v_ClipRect.y + v_ClipRect.w)) discard;
    o_EntityID = v_EntityID;
}
