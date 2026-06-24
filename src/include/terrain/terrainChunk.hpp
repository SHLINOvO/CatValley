#pragma once
#include <vector>
#include <limits>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../shader.hpp"
#include "heightmap.hpp"
#include "frustumCulling.hpp" // AABB

struct TerrainVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

class TerrainChunk {
public:
    TerrainChunk(
        Heightmap& heightmap,
        int chunkX,
        int chunkZ,
        int chunkSize,     // 顶点数，例如 33（=2^n+1）
        float gridScale
    );

    void Draw(Shader& shader, int lod);

    AABB TerrainChunk::getAABBWorld() const { return bounds; };
    glm::vec3 getCenter() const { return center; };

private:
    void buildVertices();
    void buildIndices(int step, std::vector<unsigned int>& outIndices);

    // Skirt：生成下拉副本顶点（四边），并构建每个 LOD 的 skirt 索引
    void buildSkirtVertices(float skirtDepth);
    void buildSkirtIndicesForLOD(int step, std::vector<unsigned int>& out);

    void setupMesh();
    void computeBounds(float skirtDepth);

    glm::vec3 calculateNormal(int hx, int hz) const;

private:
    Heightmap& heightmap;

    int chunkX, chunkZ;
    int chunkSize;
    float gridScale;

    std::vector<TerrainVertex> vertices;

    // 主地形 indices（LOD0~3）
    std::vector<unsigned int> indicesLOD0;
    std::vector<unsigned int> indicesLOD1;
    std::vector<unsigned int> indicesLOD2;
    std::vector<unsigned int> indicesLOD3;

    // Skirt indices（LOD0~3）
    std::vector<unsigned int> skirtIndicesLOD0;
    std::vector<unsigned int> skirtIndicesLOD1;
    std::vector<unsigned int> skirtIndicesLOD2;
    std::vector<unsigned int> skirtIndicesLOD3;

    // OpenGL
    unsigned int VAO = 0, VBO = 0;

    unsigned int EBOLOD0 = 0, EBOLOD1 = 0, EBOLOD2 = 0, EBOLOD3 = 0;
    unsigned int EBOSkirt0 = 0, EBOSkirt1 = 0, EBOSkirt2 = 0, EBOSkirt3 = 0;

    // Skirt 映射：原始边界顶点 index -> 对应 skirt 顶点 index（在 vertices 末尾）
    std::vector<int> skirtMap; // size = chunkSize*chunkSize, -1 表示无

    AABB bounds;
    glm::vec3 center;
};

// --------------------------- 实现 ---------------------------

static inline bool isBoundary(int x, int z, int N) {
    return (x == 0 || z == 0 || x == N - 1 || z == N - 1);
}

TerrainChunk::TerrainChunk(
    Heightmap& heightmap,
    int chunkX,
    int chunkZ,
    int chunkSize,
    float gridScale
)
    : heightmap(heightmap),
    chunkX(chunkX),
    chunkZ(chunkZ),
    chunkSize(chunkSize),
    gridScale(gridScale)
{
    // 你可以根据地形尺度调这个值：需要足够盖住裂缝
    const float SKIRT_DEPTH = 50.0f;

    buildVertices();

    // 1) 生成 Skirt 顶点（四边边界顶点的下拉副本）
    buildSkirtVertices(SKIRT_DEPTH);

    // 2) 生成 LOD0~3 主地形索引
    buildIndices(1, indicesLOD0);
    buildIndices(2, indicesLOD1);
    buildIndices(4, indicesLOD2);
    buildIndices(8, indicesLOD3);

    // 3) 生成 LOD0~3 Skirt 索引（四边完整）
    buildSkirtIndicesForLOD(1, skirtIndicesLOD0);
    buildSkirtIndicesForLOD(2, skirtIndicesLOD1);
    buildSkirtIndicesForLOD(4, skirtIndicesLOD2);
    buildSkirtIndicesForLOD(8, skirtIndicesLOD3);

    // 4) bounds（包含 skirt）
    computeBounds(SKIRT_DEPTH);

    // 5) 上传 GPU
    setupMesh();
}

void TerrainChunk::buildVertices() {
    vertices.resize(chunkSize * chunkSize);

    int startX = chunkX * (chunkSize - 1);
    int startZ = chunkZ * (chunkSize - 1);

    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            int hx = startX + x;
            int hz = startZ + z;

            float y = heightmap.get(hx, hz);

            TerrainVertex v;
            float halfW = (heightmap.width - 1) * gridScale * 0.5f;
            float halfH = (heightmap.height - 1) * gridScale * 0.5f;

            v.Position = glm::vec3(
                hx * gridScale - halfW,
                y,
                hz * gridScale - halfH
            );

            // 纹理坐标仍按全局 heightmap 归一化
            v.TexCoords = glm::vec2(
                float(hx) / (heightmap.width - 1),
                float(hz) / (heightmap.height - 1)
            );

            // 法线
            v.Normal = calculateNormal(hx, hz);

            vertices[z * chunkSize + x] = v;
        }
    }
}

void TerrainChunk::buildIndices(int step, std::vector<unsigned int>& out) {
    // 要求 chunkSize-1 能被 step 整除，否则最后一列/行会出界或丢面
    for (int z = 0; z < chunkSize - 1; z += step) {
        for (int x = 0; x < chunkSize - 1; x += step) {
            int tl = z * chunkSize + x;
            int tr = tl + step;
            int bl = (z + step) * chunkSize + x;
            int br = bl + step;

            out.push_back(tl);
            out.push_back(bl);
            out.push_back(tr);

            out.push_back(tr);
            out.push_back(bl);
            out.push_back(br);
        }
    }
}

void TerrainChunk::buildSkirtVertices(float skirtDepth) {
    const int N = chunkSize;

    skirtMap.assign(N * N, -1);

    // 为所有边界顶点创建下拉副本（避免 corner 重复）
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            if (!isBoundary(x, z, N)) continue;

            int baseIdx = z * N + x;

            TerrainVertex sv = vertices[baseIdx];
            sv.Position.y -= skirtDepth;

            int skirtIdx = (int)vertices.size();
            vertices.push_back(sv);
            skirtMap[baseIdx] = skirtIdx;
        }
    }
}

static inline void addSkirtQuad(
    int v0, int v1, int sv0, int sv1,
    std::vector<unsigned int>& out
) {
    // 形成一个四边形：v0-v1-sv1-sv0
    // 三角形： (v0, sv0, v1), (v1, sv0, sv1)
    out.push_back((unsigned)v0);
    out.push_back((unsigned)sv0);
    out.push_back((unsigned)v1);

    out.push_back((unsigned)v1);
    out.push_back((unsigned)sv0);
    out.push_back((unsigned)sv1);
}

void TerrainChunk::buildSkirtIndicesForLOD(int step, std::vector<unsigned int>& out) {
    const int N = chunkSize;

    // -------- 上边（z=0，x: 0->N-1）--------
    {
        int z = 0;
        for (int x = 0; x < N - step; x += step) {
            int v0 = z * N + x;
            int v1 = z * N + (x + step);
            int sv0 = skirtMap[v0];
            int sv1 = skirtMap[v1];
            addSkirtQuad(v0, v1, sv0, sv1, out);
        }
    }

    // -------- 右边（x=N-1，z: 0->N-1）--------
    {
        int x = N - 1;
        for (int z = 0; z < N - step; z += step) {
            int v0 = z * N + x;
            int v1 = (z + step) * N + x;
            int sv0 = skirtMap[v0];
            int sv1 = skirtMap[v1];
            addSkirtQuad(v0, v1, sv0, sv1, out);
        }
    }

    // -------- 下边（z=N-1，x: N-1->0）--------
    // 反向走一遍，保证环绕方向一致（不是必须，但利于统一剔除面方向）
    {
        int z = N - 1;
        for (int x = N - 1; x - step >= 0; x -= step) {
            int v0 = z * N + x;
            int v1 = z * N + (x - step);
            int sv0 = skirtMap[v0];
            int sv1 = skirtMap[v1];
            addSkirtQuad(v0, v1, sv0, sv1, out);
        }
    }

    // -------- 左边（x=0，z: N-1->0）--------
    {
        int x = 0;
        for (int z = N - 1; z - step >= 0; z -= step) {
            int v0 = z * N + x;
            int v1 = (z - step) * N + x;
            int sv0 = skirtMap[v0];
            int sv1 = skirtMap[v1];
            addSkirtQuad(v0, v1, sv0, sv1, out);
        }
    }
}

void TerrainChunk::computeBounds(float skirtDepth) {
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(-std::numeric_limits<float>::max());

    for (const auto& v : vertices) {
        mn = glm::min(mn, v.Position);
        mx = glm::max(mx, v.Position);
    }

    // 防闪烁 eps
    const float eps = 1.0f;
    mn -= glm::vec3(eps);
    mx += glm::vec3(eps);

    bounds.min = mn;
    bounds.max = mx;

    center = 0.5f * (bounds.min + bounds.max);
}

void TerrainChunk::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glGenBuffers(1, &EBOLOD0);
    glGenBuffers(1, &EBOLOD1);
    glGenBuffers(1, &EBOLOD2);
    glGenBuffers(1, &EBOLOD3);

    glGenBuffers(1, &EBOSkirt0);
    glGenBuffers(1, &EBOSkirt1);
    glGenBuffers(1, &EBOSkirt2);
    glGenBuffers(1, &EBOSkirt3);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(TerrainVertex),
        vertices.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, Position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
        sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, Normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
        sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, TexCoords));

    // 主地形 EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD0);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indicesLOD0.size() * sizeof(unsigned int),
        indicesLOD0.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD1);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indicesLOD1.size() * sizeof(unsigned int),
        indicesLOD1.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD2);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indicesLOD2.size() * sizeof(unsigned int),
        indicesLOD2.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD3);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indicesLOD3.size() * sizeof(unsigned int),
        indicesLOD3.data(),
        GL_STATIC_DRAW);

    // Skirt EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt0);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        skirtIndicesLOD0.size() * sizeof(unsigned int),
        skirtIndicesLOD0.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt1);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        skirtIndicesLOD1.size() * sizeof(unsigned int),
        skirtIndicesLOD1.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt2);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        skirtIndicesLOD2.size() * sizeof(unsigned int),
        skirtIndicesLOD2.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt3);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        skirtIndicesLOD3.size() * sizeof(unsigned int),
        skirtIndicesLOD3.data(),
        GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void TerrainChunk::Draw(Shader& shader, int lod) {
    // clamp lod
    if (lod < 0) lod = 0;
    if (lod > 3) lod = 3;

    glBindVertexArray(VAO);

    // ------- 主地形 -------
    switch (lod) {
    case 0:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD0);
        glDrawElements(GL_TRIANGLES, (GLsizei)indicesLOD0.size(), GL_UNSIGNED_INT, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt0);
        glDrawElements(GL_TRIANGLES, (GLsizei)skirtIndicesLOD0.size(), GL_UNSIGNED_INT, 0);
        break;

    case 1:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD1);
        glDrawElements(GL_TRIANGLES, (GLsizei)indicesLOD1.size(), GL_UNSIGNED_INT, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt1);
        glDrawElements(GL_TRIANGLES, (GLsizei)skirtIndicesLOD1.size(), GL_UNSIGNED_INT, 0);
        break;

    case 2:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD2);
        glDrawElements(GL_TRIANGLES, (GLsizei)indicesLOD2.size(), GL_UNSIGNED_INT, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt2);
        glDrawElements(GL_TRIANGLES, (GLsizei)skirtIndicesLOD2.size(), GL_UNSIGNED_INT, 0);
        break;

    case 3:
    default:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOLOD3);
        glDrawElements(GL_TRIANGLES, (GLsizei)indicesLOD3.size(), GL_UNSIGNED_INT, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOSkirt3);
        glDrawElements(GL_TRIANGLES, (GLsizei)skirtIndicesLOD3.size(), GL_UNSIGNED_INT, 0);
        break;
    }

    glBindVertexArray(0);
}

glm::vec3 TerrainChunk::calculateNormal(int hx, int hz) const
{
    // 使用全局 heightmap 坐标
    float hl = heightmap.get(hx - 1, hz);
    float hr = heightmap.get(hx + 1, hz);
    float hd = heightmap.get(hx, hz - 1);
    float hu = heightmap.get(hx, hz + 1);

    // X/Z 方向间距 = gridScale
    // 中央差分，y 分量相当于“竖直权重”
    glm::vec3 n(
        hl - hr,
        2.0f * gridScale,
        hd - hu
    );

    return glm::normalize(n);
}


