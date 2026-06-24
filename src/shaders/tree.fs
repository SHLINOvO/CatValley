#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 lightSpaceFragPos;

uniform vec3 viewPos;
uniform sampler2D shadowMap;

struct Light {
    vec3 direction;
    vec3 color;
};
uniform Light light;

uniform sampler2D texture_diffuse1;
uniform bool hasTexture;
uniform vec3 Material_baseColor;

// ---------- Shadow calculation with PCF ----------
float ShadowCalculation(vec4 lightSpacePos, vec3 normal, vec3 lightDir_ws)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    float bias = max(0.002 * (1.0 - dot(normal, lightDir_ws)), 0.0008);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float depth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > depth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main()
{
    vec3 albedo = Material_baseColor;

    if (hasTexture) {
        vec4 tex = texture(texture_diffuse1, TexCoords);
        // alpha cutout
        if (tex.a < 0.35) discard;
        albedo *= tex.rgb;
    }

    vec3 N = normalize(Normal);
    // double-sided leaf shading: flip normal for backfaces
    if (!gl_FrontFacing) N = -N;

    // 1) Directional light
    vec3 L = normalize(-light.direction);
    vec3 V = normalize(viewPos - FragPos);
    vec3 H = normalize(L + V);

    // 2) Shadow
    float shadow = ShadowCalculation(lightSpaceFragPos, N, L);

    // 3) Hemisphere ambient (sky/ground) - not affected by shadow
    float up = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = vec3(0.65, 0.75, 0.95);
    vec3 groundColor = vec3(0.25, 0.22, 0.18);
    vec3 hemi = mix(groundColor, skyColor, up);
    vec3 ambient = hemi * albedo * 0.75;

    // 4) Wrap diffuse - affected by shadow
    float ndl = dot(N, L);
    float wrap = 0.35;
    float diff = clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0);
    vec3 diffuse = (1.0 - shadow) * diff * albedo * light.color;

    // 5) Subsurface-ish (back lighting) - slightly affected by shadow
    float back = clamp(dot(-N, L), 0.0, 1.0);
    vec3 subsurface = (1.0 - shadow * 0.5) * back * albedo * light.color * 0.25;

    // 6) Specular (Blinn-Phong) - affected by shadow
    float specPow = 32.0;
    float spec = pow(max(dot(N, H), 0.0), specPow);
    vec3 specular = (1.0 - shadow) * spec * light.color * 0.18;

    vec3 color = ambient + diffuse + subsurface + specular;

    // overall boost (light.color is now higher, reduce boost accordingly)
    color *= 1.10;

    FragColor = vec4(color, 1.0);
}
