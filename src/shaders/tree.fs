#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 viewPos;

struct Light {
    vec3 direction;
    vec3 color;
};
uniform Light light;

uniform sampler2D texture_diffuse1;
uniform bool hasTexture;
uniform vec3 Material_baseColor;

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

    // 2) Hemisphere ambient (sky/ground)
    float up = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = vec3(0.65, 0.75, 0.95);
    vec3 groundColor = vec3(0.25, 0.22, 0.18);
    vec3 hemi = mix(groundColor, skyColor, up);
    vec3 ambient = hemi * albedo * 0.75;

    // 3) Wrap diffuse
    float ndl = dot(N, L);
    float wrap = 0.35;
    float diff = clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0);
    vec3 diffuse = diff * albedo * light.color;

    // 4) Subsurface-ish (back lighting)
    float back = clamp(dot(-N, L), 0.0, 1.0);
    vec3 subsurface = back * albedo * light.color * 0.25;

    // 5) Specular (Blinn-Phong)
    float specPow = 32.0;
    float spec = pow(max(dot(N, H), 0.0), specPow);
    vec3 specular = spec * light.color * 0.18;

    vec3 color = ambient + diffuse + subsurface + specular;

    // slight boost
    color *= 1.70;

    FragColor = vec4(color, 1.0);
}
