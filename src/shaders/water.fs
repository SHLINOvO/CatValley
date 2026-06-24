#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec4 lightSpaceFragPos;
    vec4 clipSpacePos;   // screen-space clip coords for reflection texture lookup
} fs_in;

// ========== Textures ==========
uniform sampler2D reflectionTexture;  // 反射 FBO 颜色纹理
uniform sampler2D shadowMap;

// ========== Params ==========
uniform vec3 viewPos;         // 摄像机世界位置
uniform vec3 lightDir;        // surface -> sun
uniform vec3 lightColor;      // 太阳光 radiance
uniform float time;           // 波浪动画时间
uniform float waterHeight;    // 水面高度

// ---------- 水面参数 ----------
const vec3 deepWaterColor = vec3(0.02, 0.12, 0.22);   // 深水蓝
const vec3 shallowColor   = vec3(0.04, 0.28, 0.35);   // 浅水蓝绿
const float fresnelPower  = 3.0;                        // Fresnel 幂次
const vec3 F0_water       = vec3(0.02);                 // 水面 F0（低反射率）
const float waveDistortA  = 0.02;                        // 反射UV 扭曲幅度
const float waveDistortB  = 0.01;                        // 第二层扭曲

// ---------- Shadow ----------
float ShadowCalculation(vec4 lightSpacePos) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    float bias = 0.003;
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
    // ---------- 水面法线（带波浪扰动） ----------
    // 用差分方式从波浪函数计算近似法线
    float eps = 0.5;
    float h0 = sin(fs_in.FragPos.x * 0.15 + time * 1.2) * 0.15
             + sin(fs_in.FragPos.z * 0.2  + time * 0.8) * 0.10
             + sin((fs_in.FragPos.x + fs_in.FragPos.z) * 0.1 + time * 1.5) * 0.08;
    float hx = sin((fs_in.FragPos.x + eps) * 0.15 + time * 1.2) * 0.15
             + sin(fs_in.FragPos.z * 0.2  + time * 0.8) * 0.10
             + sin((fs_in.FragPos.x + eps + fs_in.FragPos.z) * 0.1 + time * 1.5) * 0.08;
    float hz = sin(fs_in.FragPos.x * 0.15 + time * 1.2) * 0.15
             + sin((fs_in.FragPos.z + eps) * 0.2  + time * 0.8) * 0.10
             + sin((fs_in.FragPos.x + fs_in.FragPos.z + eps) * 0.1 + time * 1.5) * 0.08;
    vec3 N = normalize(vec3(-(hx - h0) / eps, 1.0, -(hz - h0) / eps));

    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 L = normalize(lightDir);  // surface -> sun

    // ---------- Fresnel ----------
    float NdV = max(dot(N, V), 0.0);
    // Schlick Fresnel: 掠射角反射更强
    float fresnel = pow(clamp(1.0 - NdV, 0.0, 1.0), fresnelPower);
    // 最低反射率保证（即使正视也有少量反射）
    fresnel = max(fresnel, 0.06);

    // ---------- 反射纹理采样 ----------
    // 用屏幕空间坐标（主摄像机投影）采样反射 FBO 纹理，
    // 而非光源空间坐标——反射 FBO 是用主摄像机投影渲染的
    vec2 projUV = fs_in.clipSpacePos.xy / fs_in.clipSpacePos.w;
    projUV = projUV * 0.5 + 0.5;

    // 波浪扰动 UV
    vec2 distortA = vec2(
        sin(fs_in.FragPos.x * 0.3 + time * 2.0) * waveDistortA,
        sin(fs_in.FragPos.z * 0.3 + time * 1.5) * waveDistortA
    );
    vec2 distortB = vec2(
        cos(fs_in.FragPos.x * 0.5 + time * 1.0) * waveDistortB,
        cos(fs_in.FragPos.z * 0.4 + time * 0.7) * waveDistortB
    );

    vec2 reflUV = projUV + distortA + distortB;
    reflUV = clamp(reflUV, 0.001, 0.999);

    vec3 reflectionColor = texture(reflectionTexture, reflUV).rgb;

    // ---------- 深水颜色 ----------
    // 深度因子：中心深、边缘浅
    float distFromCenter = length(fs_in.FragPos.xz - vec2(80.0, 80.0));
    float depthFactor = clamp(1.0 - distFromCenter / 50.0, 0.0, 1.0);
    vec3 waterBaseColor = mix(shallowColor, deepWaterColor, depthFactor);

    // ---------- 折射颜色 ----------
    // 折射 = 深水基底色（透过水面看下面的深色）
    vec3 refractionColor = waterBaseColor;

    // ---------- Fresnel 混合：反射 vs 折射 ----------
    vec3 waterColor = mix(refractionColor, reflectionColor, fresnel);

    // ---------- 高光 ----------
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 256.0);  // 镜面高光
    float shadow = ShadowCalculation(fs_in.lightSpaceFragPos);
    vec3 specular = (1.0 - shadow) * spec * lightColor * 0.6;

    // ---------- 合成 ----------
    // 散射光照
    float NdL = max(dot(N, L), 0.0);
    vec3 diffuse = (1.0 - shadow) * NdL * waterBaseColor * 0.3;

    // 半球环境光
    float upFactor = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyAmb  = vec3(0.45, 0.55, 0.70);
    vec3 gndAmb  = vec3(0.20, 0.17, 0.12);
    vec3 ambient = mix(gndAmb, skyAmb, upFactor) * waterBaseColor * 0.5;

    vec3 color = waterColor + diffuse + ambient + specular;

    // Tone mapping + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    // Alpha: 深处更不透明，浅处更透明
    float alpha = mix(0.55, 0.85, depthFactor);

    FragColor = vec4(color, alpha);
}
