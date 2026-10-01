#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_TexCoord;
flat out int v_TexIndex;
out float v_TilingFactor;

void main()
{
    v_Color = a_Color;
    v_TexCoord = a_TexCoord;
    v_TexIndex = int(a_TexIndex);
    v_TilingFactor = a_TilingFactor;

    gl_Position =
        u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec4 v_Color;
in vec2 v_TexCoord;
flat in int v_TexIndex;
in float v_TilingFactor;

uniform sampler2D u_Textures[16];

void main()
{
    vec2 uv = v_TexCoord * v_TilingFactor;
    vec4 texColor = vec4(1.0);

    switch (v_TexIndex)
    {
        case 0:  texColor = texture(u_Textures[0],  uv); break;
        case 1:  texColor = texture(u_Textures[1],  uv); break;
        case 2:  texColor = texture(u_Textures[2],  uv); break;
        case 3:  texColor = texture(u_Textures[3],  uv); break;
        case 4:  texColor = texture(u_Textures[4],  uv); break;
        case 5:  texColor = texture(u_Textures[5],  uv); break;
        case 6:  texColor = texture(u_Textures[6],  uv); break;
        case 7:  texColor = texture(u_Textures[7],  uv); break;
        case 8:  texColor = texture(u_Textures[8],  uv); break;
        case 9:  texColor = texture(u_Textures[9],  uv); break;
        case 10: texColor = texture(u_Textures[10], uv); break;
        case 11: texColor = texture(u_Textures[11], uv); break;
        case 12: texColor = texture(u_Textures[12], uv); break;
        case 13: texColor = texture(u_Textures[13], uv); break;
        case 14: texColor = texture(u_Textures[14], uv); break;
        case 15: texColor = texture(u_Textures[15], uv); break;
    }

    color = texColor * v_Color;
}