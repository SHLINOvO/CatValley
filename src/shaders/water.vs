#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec4 lightSpaceFragPos;
    vec4 clipSpacePos;   // screen-space clip coords for reflection texture lookup
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform float time;          // 波浪动画时间
uniform float waterHeight;   // 水面Y高度（用于波浪振幅）
uniform float clipPlaneY;    // 反射 Pass 裁剪平面

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);

    // 小波浪：多频率叠加
    float wave1 = sin(worldPos.x * 0.15 + time * 1.2) * 0.15;
    float wave2 = sin(worldPos.z * 0.2  + time * 0.8) * 0.10;
    float wave3 = sin((worldPos.x + worldPos.z) * 0.1 + time * 1.5) * 0.08;
    worldPos.y += wave1 + wave2 + wave3;

    vs_out.FragPos = worldPos.xyz;
    vs_out.TexCoords = aTexCoords;
    vs_out.lightSpaceFragPos = lightSpaceMatrix * worldPos;

    // Clip plane for reflection pass
    gl_ClipDistance[0] = worldPos.y - clipPlaneY;

    gl_Position = projection * view * worldPos;
    vs_out.clipSpacePos = gl_Position;
}
