#pragma once
#include <string>
#include "../shader.hpp"
#include "terrainSystem.hpp"

// 你原先的 loadTexture2D 保留（建议你自己额外加 data==nullptr 的保护，这里按你现有风格不改）
unsigned int loadTexture2D(const std::string& path)
{
    unsigned int texID;
    glGenTextures(1, &texID);

    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);

    GLenum format = (ch == 1) ? GL_RED :
        (ch == 3) ? GL_RGB : GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format,
        GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texID;
}

class Terrain {
public:
    Terrain(
        const std::string& heightmapPath,
        float heightScale,
        int chunkCountX, int chunkCountZ, int chunkSize, float gridScale
    );

    ~Terrain();

    void setLODDistances(float d0, float d1, float d2);
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

    float getHeightWorld(float worldX, float worldZ) const;
    glm::vec3 getNormalWorld(float worldX, float worldZ) const;
    float getSlopeRadians(float worldX, float worldZ) const;
    float getSlopeDegrees(float worldX, float worldZ) const;

private:
    void loadTextures();
    void setupShader();

    Heightmap heightmap;
    TerrainSystem terrainSystem;
    Shader terrainShader;

    // ---------- Textures ----------
    unsigned int grassLowTex = 0; // 低海拔草
    unsigned int grassHighTex = 0; // 高海拔草(你原来的 grass_diff_2)
    unsigned int noiseTex = 0;

    // ---------- Material / Blend Params ----------
    float uvScale = 32.0f;

    // 高度控制：低草 -> 高草
    float grassLowMaxHeight = 1080.0f; // <= 基本都是低草
    float grassHighMinHeight = 1100.0f; // >= 基本都是高草

    // 混合带宽（世界高度单位）：越大越“糊”越自然
    float blendWidth = 30.0f; // 建议 6~30，根据地形高度尺度调

    // 噪声参数：用于“扰动过渡边界”，让过渡碎裂自然
    float blendNoiseScale = 0.05f;  // 低频噪声
    float blendNoiseAmp = 0.25f;  // 0~1，建议 0.15~0.5

    // 过渡曲线：>1 会让过渡更集中/更自然（可选）
    float blendPower = 1.2f;
};

inline Terrain::Terrain(
    const std::string& heightmapPath,
    float heightScale,
    int chunkCountX, int chunkCountZ, int chunkSize, float gridScale
)
    : heightmap(heightmapPath, heightScale),
    terrainSystem(heightmap, chunkCountX, chunkCountZ, chunkSize, gridScale),
    terrainShader(SHADERS_FOLDER "terrain.vs", SHADERS_FOLDER "terrain.fs")
{
    loadTextures();
    setupShader();
}

inline void Terrain::loadTextures() {
    grassLowTex = loadTexture2D(ASSETS_FOLDER "terrain/grass_diff.png");
    grassHighTex = loadTexture2D(ASSETS_FOLDER "terrain/grass_diff_2.png");
    noiseTex = loadTexture2D(ASSETS_FOLDER "terrain/noise.png");
}

inline void Terrain::setupShader() {
    terrainShader.use();

    // sampler2D bindings
    terrainShader.setInt("grassLowTex", 0);
    terrainShader.setInt("grassHighTex", 1);
    terrainShader.setInt("noiseTex", 2);

    // base tiling
    terrainShader.setFloat("uvScale", uvScale);

    // height thresholds
    terrainShader.setFloat("grassLowMaxHeight", grassLowMaxHeight);
    terrainShader.setFloat("grassHighMinHeight", grassHighMinHeight);

    // better blend controls
    terrainShader.setFloat("blendWidth", blendWidth);
    terrainShader.setFloat("blendNoiseScale", blendNoiseScale);
    terrainShader.setFloat("blendNoiseAmp", blendNoiseAmp);
    terrainShader.setFloat("blendPower", blendPower);
}

inline void Terrain::setLODDistances(float d0, float d1, float d2) {
    terrainSystem.setLODDistances(d0, d1, d2);
}

inline void Terrain::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
    terrainShader.use();

    glm::mat4 model = glm::mat4(1.0f);
    terrainShader.setMat4("model", model);
    terrainShader.setMat4("view", view);
    terrainShader.setMat4("projection", projection);

    // Directional light (world space)
    glm::vec3 dir = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.4f));
    glm::vec3 color = glm::vec3(1.0f);
    terrainShader.setVec3("lightDir", dir);
    terrainShader.setVec3("lightColor", color);

    // Bind textures
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, grassLowTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, grassHighTex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTex);

    terrainSystem.Draw(
        terrainShader,
        model,
        view,
        projection,
        cameraPos
    );
}

inline float Terrain::getHeightWorld(float worldX, float worldZ) const {
    return terrainSystem.getHeightWorld(worldX, worldZ);
}

inline glm::vec3 Terrain::getNormalWorld(float worldX, float worldZ) const {
    return terrainSystem.getNormalWorld(worldX, worldZ);
}

inline float Terrain::getSlopeRadians(float worldX, float worldZ) const {
    return terrainSystem.getSlopeRadians(worldX, worldZ);
}

inline float Terrain::getSlopeDegrees(float worldX, float worldZ) const {
    return terrainSystem.getSlopeDegrees(worldX, worldZ);
}

inline Terrain::~Terrain() {
    glDeleteTextures(1, &grassLowTex);
    glDeleteTextures(1, &grassHighTex);
    glDeleteTextures(1, &noiseTex);
}
