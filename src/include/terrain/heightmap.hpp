// Heightmap.hpp
#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

class Heightmap {
public:
    int width = 0;
    int height = 0;
    int channels = 0;
    float heightScale = 1.0f;
    float gridScale = 1.0f;

    std::vector<float> heightData;

    Heightmap(const std::string& path, float heightScale)
        : heightScale(heightScale)
    {
        load(path);
    }

    float get(int x, int z) const {
        x = glm::clamp(x, 0, width - 1);
        z = glm::clamp(z, 0, height - 1);
        return heightData[z * width + x] * heightScale;
    }

    // 在内存中挖一个圆形湖泊凹陷
    // centerX, centerZ: 世界坐标（需要转换为像素坐标）
    // radius: 世界半径
    // depth: 世界高度降低量（正值 = 挖下去）
    // blendWidth: 边缘渐变宽度（世界单位，平滑过渡）
    // gridScale: 世界-像素缩放因子
    void carveLake(float centerX, float centerZ, float radius, float depth, float blendWidth, float gs) {
        gridScale = gs;
        // 世界坐标转像素坐标（heightmap像素 0~width-1 对应世界 -halfSize ~ +halfSize）
        float halfSize = (width - 1) * gs * 0.5f;
        float pixCX = (centerX + halfSize) / gs;
        float pixCZ = (centerZ + halfSize) / gs;
        float pixR  = radius / gs;
        float pixBlend = blendWidth / gs;

        // 先记录原始湖心高度（用于确定水面水位）
        int cxPx = glm::clamp(static_cast<int>(pixCX), 0, width - 1);
        int czPx = glm::clamp(static_cast<int>(pixCZ), 0, height - 1);
        float originalCenterH = heightData[czPx * width + cxPx];  // normalized 0~1

        // depth 转换为 normalized 0~1 单位
        float depthNorm = depth / heightScale;

        // 遍历所有在范围内的像素
        int xMin = glm::clamp(static_cast<int>(pixCX - pixR - pixBlend), 0, width - 1);
        int xMax = glm::clamp(static_cast<int>(pixCX + pixR + pixBlend), 0, width - 1);
        int zMin = glm::clamp(static_cast<int>(pixCZ - pixR - pixBlend), 0, height - 1);
        int zMax = glm::clamp(static_cast<int>(pixCZ + pixR + pixBlend), 0, height - 1);

        for (int z = zMin; z <= zMax; ++z) {
            for (int x = xMin; x <= xMax; ++x) {
                float dx = static_cast<float>(x) - pixCX;
                float dz = static_cast<float>(z) - pixCZ;
                float dist = std::sqrt(dx * dx + dz * dz);

                if (dist > pixR + pixBlend) continue;

                // 形状因子：0=湖心最深，1=边缘
                float shape = 1.0f;
                if (dist <= pixR) {
                    shape = dist / pixR;  // 线性：中心最深
                }

                // 边缘过渡：pixR ~ pixR+pixBlend 范围平滑
                float edgeBlend = 1.0f;
                if (dist > pixR) {
                    edgeBlend = 1.0f - (dist - pixR) / pixBlend;  // 1→0 渐变
                    edgeBlend = glm::clamp(edgeBlend, 0.0f, 1.0f);
                    shape = 1.0f;  // 过渡区域深度=0
                }

                float localDepth = depthNorm * (1.0f - shape) * edgeBlend;

                int idx = z * width + x;
                heightData[idx] = std::max(heightData[idx] - localDepth, 0.0f);
            }
        }
    }

private:
    void load(const std::string& path);
};

void Heightmap::load(const std::string& path) {
    unsigned short* data = stbi_load_16(
        path.c_str(),
        &width,
        &height,
        &channels,
        1   // 强制单通道
    );

    if (!data) {
        std::cerr << "Failed to load heightmap: " << path << std::endl;
        return;
    }

    heightData.resize(width * height);

    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            int index = z * width + x;
            heightData[index] = data[index] / 65535.0f;
        }
    }

    stbi_image_free(data);
}
