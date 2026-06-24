#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 lightSpaceFragPos;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform float clipPlaneY;  // 反射Pass裁剪（设为极大值=不裁剪）

void main()
{
    vec4 FragPos = model * vec4(aPos, 1.0);
    vs_out.FragPos = FragPos.xyz;
    vs_out.Normal = mat3(transpose(inverse(model))) * aNormal;
    vs_out.TexCoords = aTexCoords;
    vs_out.lightSpaceFragPos = lightSpaceMatrix * FragPos;

    gl_ClipDistance[0] = FragPos.y - clipPlaneY;

    gl_Position = projection * view * FragPos;
}
