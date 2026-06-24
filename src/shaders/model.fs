#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform vec3 Material_baseColor;
uniform bool hasTexture;

void main()
{    
    vec3 result;
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5)); // 调整光源方向
    vec3 norm = normalize(Normal);

    if(hasTexture) {
        vec4 texColor = texture(texture_diffuse1, TexCoords);
        
        result = mix(Material_baseColor, texColor.rgb, texColor.a);
    } else {
        result = Material_baseColor;
    }

    // 环境光
    vec3 ambient = 0.5 * result;
    
    // 漫反射
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * result;

    // 简单的高光
    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0));
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = 0.2 * spec * vec3(1.0); 

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}