#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

// ========== Textures ==========
uniform sampler2D grassLowTex;
uniform sampler2D grassHighTex;
uniform sampler2D noiseTex;

// ========== Params ==========
uniform float uvScale;

// Height thresholds: low -> high
uniform float grassLowMaxHeight;
uniform float grassHighMinHeight;

// Blend controls
uniform float blendWidth;      // transition band width in world height units
uniform float blendNoiseScale; // noise frequency (world xz)
uniform float blendNoiseAmp;   // noise influence (0~1)
uniform float blendPower;      // curve shaping (>=1)

// Lighting
uniform vec3 lightDir;   // directional light direction (world)
uniform vec3 lightColor;

void main()
{
    // ---------- Normal ----------
    vec3 N = normalize(fs_in.Normal);

    // ---------- Base UV ----------
    vec2 uv = fs_in.TexCoords * uvScale;

    // ---------- Sample textures ----------
    vec3 lowCol  = texture(grassLowTex,  uv).rgb;
    vec3 highCol = texture(grassHighTex, uv).rgb;

    // ---------- Compute blend factor ----------
    // 过渡中心高度：两阈值的中点
    float center = 0.5 * (grassLowMaxHeight + grassHighMinHeight);

    // 过渡半宽：避免直接用两个阈值做 smoothstep 产生“色带”
    float halfW = 0.5 * blendWidth;

    // 低频噪声用于扰动边界（让过渡自然碎裂）
    float n = texture(noiseTex, fs_in.FragPos.xz * blendNoiseScale).r;
    n = n * 2.0 - 1.0; // [-1, 1]

    // 用噪声扰动“过渡判断高度”
    float h2 = fs_in.FragPos.y + n * (blendNoiseAmp * blendWidth);

    // factor: 0 -> low grass, 1 -> high grass
    float factor = smoothstep(center - halfW, center + halfW, h2);

    factor = pow(clamp(factor, 0.0, 1.0), blendPower);

    // ---------- Blend ----------
    vec3 albedo = mix(lowCol, highCol, factor);

    // ---------- Lighting (Lambert + ambient) ----------
    float diff = max(dot(N, -normalize(lightDir)), 0.0);
    vec3 direct = diff * lightColor;
    vec3 ambient = 0.12 * lightColor; // 可调：0.08~0.25

    vec3 color = albedo * (direct + ambient);

    FragColor = vec4(color, 1.0);
}
