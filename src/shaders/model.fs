#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in vec4 lightSpaceFragPos;

uniform sampler2D shadowMap;
uniform sampler2D texture_diffuse1;
uniform vec3 Material_baseColor;
uniform bool hasTexture;

// PBR material params (set from C++ per-draw or use defaults)
uniform float metallic;   // 0 = dielectric, 1 = metal
uniform float roughness;  // 0 = mirror, 1 = fully rough

// Lighting
uniform vec3 lightDir;    // surface -> sun, world space (normalized)
uniform vec3 lightColor;  // sunlight radiance (use ~3-5 for outdoor)
uniform vec3 viewPos;     // camera world position

// Exposure control
uniform float exposure;   // brightness multiplier before tone mapping

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

// ---------- Cook-Torrance BRDF helpers ----------
const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float rough)
{
    float a  = rough * rough;
    float a2 = a * a;
    float NdH  = max(dot(N, H), 0.0);
    float NdH2 = NdH * NdH;
    float denom = NdH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith's Schlick-GGX Geometry function
float GeometrySchlickGGX(float NdV, float rough)
{
    float r = rough + 1.0;
    float k = (r * r) / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float rough)
{
    float NdV = max(dot(N, V), 0.0);
    float NdL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdV, rough) * GeometrySchlickGGX(NdL, rough);
}

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    // ---------- Albedo ----------
    vec3 albedo;
    if (hasTexture) {
        vec4 texColor = texture(texture_diffuse1, TexCoords);
        albedo = mix(Material_baseColor, texColor.rgb, texColor.a);
    } else {
        albedo = Material_baseColor;
    }

    // Gamma-correct albedo: assume texture is sRGB stored
    albedo = pow(albedo, vec3(2.2));

    // ---------- Vectors ----------
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightDir);          // surface -> sun
    vec3 H = normalize(L + V);

    // ---------- Shadow ----------
    float shadow = ShadowCalculation(lightSpaceFragPos, N, L);

    // ---------- PBR ----------
    // F0: base reflectance at normal incidence
    // dielectrics ~ 0.04, metals use albedo color
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NdL = max(dot(N, L), 0.0);
    float NdV = max(dot(N, V), 0.0);

    // Cook-Torrance specular BRDF
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  numerator   = D * G * F;
    float denominator = 4.0 * NdV * NdL + 0.0001;
    vec3  specular    = numerator / denominator;

    // Energy conservation: kS = fresnel, kD = diffuse contribution
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic); // metals have no diffuse

    // Combine: radiance of directional light (no attenuation)
    vec3 radiance = lightColor;
    vec3 directLight = (kD * albedo / PI + specular) * radiance * NdL;

    // Apply shadow to direct light only
    directLight *= (1.0 - shadow);

    // Ambient: hemisphere irradiance + proper metal handling
    // Metals reflect ambient specularly with F0 color; dielectrics scatter diffusely with albedo
    float upFactor = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyIrradiance    = vec3(0.85, 0.95, 1.10);   // bright sky
    vec3 groundIrradiance = vec3(0.40, 0.35, 0.28);   // warm ground
    vec3 hemiIrradiance   = mix(groundIrradiance, skyIrradiance, upFactor);

    // Ambient diffuse (dielectrics) + ambient specular reflection (metals)
    vec3 ambientDiff = kD * albedo / PI * hemiIrradiance;
    vec3 ambientSpec = F0 * hemiIrradiance * 0.5;      // metals reflect ambient with their color
    vec3 ambient = ambientDiff + ambientSpec;

    vec3 color = ambient + directLight;

    // Exposure control (boost before tone mapping)
    color *= exposure;

    // Reinhard tone-mapping + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
