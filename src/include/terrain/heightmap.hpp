// Heightmap.hpp
#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Heightmap {
public:
    int width = 0;
    int height = 0;
    int channels = 0;
    float heightScale = 1.0f;

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
