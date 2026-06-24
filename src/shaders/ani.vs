#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 这里的 aPos 已经是 CPU 计算完 Morph 偏移后的位置
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // 传导法线（简单处理，未考虑非统一缩放）
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}