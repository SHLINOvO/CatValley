#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "terrainChunk.hpp"
#include "frustumCulling.hpp"
#include "heightmap.hpp"
#include "../shader.hpp"

class TerrainSystem {
public:
    TerrainSystem(
        Heightmap& heightmap,
        int chunkCountX,
        int chunkCountZ,
        int chunkSize,
        float gridScale
    )
        : heightmap(heightmap), gridScale(gridScale)
    {
        chunks.reserve(chunkCountX * chunkCountZ);

        worldSizeX = chunkCountX * (chunkSize - 1) * gridScale;
        worldSizeZ = chunkCountZ * (chunkSize - 1) * gridScale;


        for (int z = 0; z < chunkCountZ; ++z) {
            for (int x = 0; x < chunkCountX; ++x) {
                chunks.emplace_back(
                    heightmap,
                    x,
                    z,
                    chunkSize,
                    gridScale
                );
            }
        }
    }

    // ==================== 主绘制接口 ====================
    // viewProj = projection * view
    void Draw(
        Shader& shader,
        const glm::mat4& model,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& cameraPos
    )
    {
        // 1. 更新视锥
        glm::mat4 viewProj = projection * view;
        frustum.updateFromMatrix(viewProj);


        // 2. 遍历 Chunk
        for (auto& chunk : chunks) {

            // ---- 视锥裁剪 ----
            if (!frustum.intersects(chunk.getAABBWorld()))
                continue;


            // ---- LOD 选择 ----
            float distance = glm::distance(cameraPos, chunk.getCenter());
            int lod = pickLOD(distance);

            // ---- 绘制 ----
            chunk.Draw(shader, lod);
        }
    }

    // ==================== LOD 参数接口 ====================
    void setLODDistances(
        float lod0,
        float lod1,
        float lod2
    )
    {
        lod0Distance = lod0;
        lod1Distance = lod1;
        lod2Distance = lod2;
    }

    float getWorldSizeX() const { return worldSizeX; }
    float getWorldSizeZ() const { return worldSizeZ; }


    // ===================== 世界高度查询 ========================
    // 输入世界坐标(x, z)，输出世界高度y
    // ===========================================================
    float getHeightWorld(float worldX, float worldZ) const {
        // 世界坐标 → heightmap 网格坐标
        float halfW = (heightmap.width - 1) * 0.5f;
        float halfH = (heightmap.height - 1) * 0.5f;

        //float gridX = worldX / gridScale + halfW;
        //float gridZ = worldZ / gridScale + halfH;

        float gridX = (worldX + halfW) / gridScale;
        float gridZ = (worldZ + halfH) / gridScale;


        // 越界检查
        if (gridX < 0.0f || gridZ < 0.0f ||
            gridX > heightmap.width - 1 ||
            gridZ > heightmap.height - 1)
        {
            return 0.0f; // 或你需要的默认高度
        }

        int x0 = static_cast<int>(std::floor(gridX));
        int z0 = static_cast<int>(std::floor(gridZ));
        int x1 = std::min(x0 + 1, heightmap.width - 1);
        int z1 = std::min(z0 + 1, heightmap.height - 1);

        float sx = gridX - x0;
        float sz = gridZ - z0;

        float h00 = heightmap.get(x0, z0);
        float h10 = heightmap.get(x1, z0);
        float h01 = heightmap.get(x0, z1);
        float h11 = heightmap.get(x1, z1);

        float h0 = glm::mix(h00, h10, sx);
        float h1 = glm::mix(h01, h11, sx);
        return glm::mix(h0, h1, sz);
    }

    // ===================== 世界法线查询 ========================
    // 输入世界坐标(x, z)，输出该点的法线
    // ===========================================================
    glm::vec3 getNormalWorld(float worldX, float worldZ) const
    {
        // 世界 → heightmap 网格坐标
        float halfW = (heightmap.width - 1) * 0.5f;
        float halfH = (heightmap.height - 1) * 0.5f;

        //float gridX = worldX / gridScale + halfW;
        //float gridZ = worldZ / gridScale + halfH;

        float gridX = (worldX + halfW) / gridScale;
        float gridZ = (worldZ + halfH) / gridScale;

        if (gridX < 0.0f || gridZ < 0.0f ||
            gridX > heightmap.width - 1 ||
            gridZ > heightmap.height - 1)
        {
            return glm::vec3(0, 1, 0);
        }

        int x = static_cast<int>(std::floor(gridX));
        int z = static_cast<int>(std::floor(gridZ));

        // 中央差分
        float hl = heightmap.get(x - 1, z);
        float hr = heightmap.get(x + 1, z);
        float hd = heightmap.get(x, z - 1);
        float hu = heightmap.get(x, z + 1);

        glm::vec3 n(
            hl - hr,
            2.0f * gridScale,
            hd - hu
        );

        return glm::normalize(n);
    }

    // ===================== 世界坡度查询 ========================
    // 输入世界坐标(x, z)，输出该点的坡度（弧度）
    // ===========================================================
    float getSlopeRadians(float worldX, float worldZ) const
    {
        glm::vec3 n = getNormalWorld(worldX, worldZ);

        // 与“世界上方向”的夹角
        float cosTheta = glm::clamp(
            glm::dot(n, glm::vec3(0, 1, 0)),
            -1.0f,
            1.0f
        );

        return std::acos(cosTheta);
    }

    // ===================== 世界坡度查询 ========================
    // 输入世界坐标(x, z)，输出该点的坡度（角度）
    // ===========================================================
    float getSlopeDegrees(float worldX, float worldZ) const
    {
        return glm::degrees(getSlopeRadians(worldX, worldZ));
    }




private:
    // ==================== LOD 选择逻辑 ====================
    int pickLOD(float distance) const
    {
        if (distance < lod0Distance)
            return 0;   // LOD0
        if (distance < lod1Distance)
            return 1;   // LOD1
        if (distance < lod2Distance)
            return 2;   // LOD2
        return 3;       // LOD3
    }

private:
    Heightmap& heightmap;
    std::vector<TerrainChunk> chunks;

    Frustum frustum;

    float gridScale;

    // LOD 距离阈值（世界单位）
    float lod0Distance = 200.0f;
    float lod1Distance = 600.0f;
    float lod2Distance = 1400.0f;

    float worldSizeX;
    float worldSizeZ;
};
