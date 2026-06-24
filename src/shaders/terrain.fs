#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 lightSpaceFragPos;
} fs_in;

// ========== Textures ==========
uniform sampler2D grassLowTex;
uniform sampler2D grassHighTex;
uniform sampler2D noiseTex;
uniform sampler2D shadowMap;

// ========== Params ==========
uniform float uvScale;
uniform float grassLowMaxHeight;
uniform float grassHighMinHeight;
uniform float blendWidth;
uniform float blendNoiseScale;
uniform float blendNoiseAmp;
uniform float blendPower;

// Lighting
uniform vec3 lightDir;    // world-space, points INTO ground (terrain convention)
uniform vec3 lightColor;  // outdoor sunlight radiance
uniform vec3 viewPos;     // camera world position

// ---------- Shadow calculation with PCF ----------
float ShadowCalculation(vec4 lightSpacePos, vec3 normal, vec3 L)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    float bias = max(0.002 * (1.0 - dot(normal, L)), 0.0008);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float depth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > depth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main()
{
    // ---------- Normal & vectors ----------
    vec3 N = normalize(fs_in.Normal);
    // lightDir in terrain.hpp is stored as -sun direction (pointing INTO ground),
    // so L = -lightDir points surface->sun
    vec3 L = normalize(-lightDir);
    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 H = normalize(L + V);

    // ---------- Base UV ----------
    vec2 uv = fs_in.TexCoords * uvScale;

    // ---------- Sample textures ----------
    vec3 lowCol  = texture(grassLowTex,  uv).rgb;
    vec3 highCol = texture(grassHighTex, uv).rgb;

    // ---------- Compute blend factor ----------
    float center = 0.5 * (grassLowMaxHeight + grassHighMinHeight);
    float halfW  = 0.5 * blendWidth;
    float n = texture(noiseTex, fs_in.FragPos.xz * blendNoiseScale).r * 2.0 - 1.0;
    float h2 = fs_in.FragPos.y + n * (blendNoiseAmp * blendWidth);
    float factor = pow(clamp(smoothstep(center - halfW, center + halfW, h2), 0.0, 1.0), blendPower);

    vec3 albedo = mix(lowCol, highCol, factor);

    // ---------- Shadow ----------
    float shadow = ShadowCalculation(fs_in.lightSpaceFragPos, N, L);

    // ---------- Lighting: Lambert diffuse + Blinn-Phong specular + hemisphere ambient ----------
    float NdL = max(dot(N, L), 0.0);

    // Diffuse (use lightColor radiance directly — terrain doesn't need /PI for Lambert)
    vec3 diffuse = (1.0 - shadow) * NdL * lightColor;

    // Blinn-Phong specular (visible but not overwhelming for grass)
    float spec    = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = (1.0 - shadow) * spec * lightColor * 0.12;

    // Hemisphere ambient: sky blue above, warm brown below
    float upFactor = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3  skyAmb   = vec3(0.45, 0.55, 0.70);  // blue sky
    vec3  gndAmb   = vec3(0.20, 0.17, 0.12);  // warm brown ground
    vec3  ambient  = mix(gndAmb, skyAmb, upFactor);

    vec3 color = albedo * (ambient + diffuse + specular);

    // Simple gamma correction (no tone mapping needed — values stay in reasonable range)
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
