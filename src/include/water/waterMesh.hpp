#pragma once
#include <vector>
#include <cmath>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "../shader.hpp"

class WaterMesh {
public:
    float centerX, centerZ, radius, waterY;
    GLuint VAO, VBO, EBO;
    int indexCount;

    WaterMesh(float cx, float cz, float r, float wy, int segments = 64)
        : centerX(cx), centerZ(cz), radius(r), waterY(wy), indexCount(0)
    {
        generateMesh(segments);
    }

    ~WaterMesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    void Draw() {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

private:
    void generateMesh(int segments) {
        // 从中心向外辐射的同心圆网格 + 三角形扇面
        // rings: 同心环数（从中心到边缘）
        int rings = 16;

        std::vector<float> vertices;  // x, y, z, u, v (5 floats per vertex)

        // 中心点
        vertices.push_back(centerX);  // x
        vertices.push_back(waterY);   // y
        vertices.push_back(centerZ);  // z
        vertices.push_back(0.5f);     // u
        vertices.push_back(0.5f);     // v

        // 各环各段顶点
        for (int r = 1; r <= rings; ++r) {
            float ringRatio = static_cast<float>(r) / rings;
            float ringRadius = radius * ringRatio;
            for (int s = 0; s < segments; ++s) {
                float angle = 2.0f * glm::pi<float>() * s / segments;
                float x = centerX + ringRadius * std::cos(angle);
                float z = centerZ + ringRadius * std::sin(angle);
                float u = 0.5f + 0.5f * ringRatio * std::cos(angle);
                float v = 0.5f + 0.5f * ringRatio * std::sin(angle);
                vertices.push_back(x);
                vertices.push_back(waterY);
                vertices.push_back(z);
                vertices.push_back(u);
                vertices.push_back(v);
            }
        }

        // 索引：三角形扇面
        std::vector<unsigned int> indices;

        // 第一环：中心到第一环
        for (int s = 0; s < segments; ++s) {
            int nextS = (s + 1) % segments;
            // 中心=0, 第一环顶点: 1+s, 1+nextS
            indices.push_back(0);
            indices.push_back(1 + s);
            indices.push_back(1 + nextS);
        }

        // 后续环：上一环到当前环
        for (int r = 1; r < rings; ++r) {
            int innerStart = 1 + (r - 1) * segments;
            int outerStart = 1 + r * segments;
            for (int s = 0; s < segments; ++s) {
                int nextS = (s + 1) % segments;
                // 四边形拆成两个三角形
                indices.push_back(innerStart + s);
                indices.push_back(outerStart + s);
                indices.push_back(outerStart + nextS);

                indices.push_back(innerStart + s);
                indices.push_back(outerStart + nextS);
                indices.push_back(innerStart + nextS);
            }
        }

        indexCount = static_cast<int>(indices.size());

        // OpenGL setup
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // position: location 0 (x,y,z)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        // uv: location 2 (matching terrain shader layout)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);
    }
};
