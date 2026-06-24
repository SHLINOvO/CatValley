#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <array>
#include <algorithm>

// ====================== AABB ======================
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

// ====================== Plane ======================
struct Plane {
    glm::vec3 n;  // normal
    float d;      // distance: n·x + d = 0

    // normalize plane
    void normalize() {
        float len = glm::length(n);
        if (len > 0.0f) {
            n /= len;
            d /= len;
        }
    }

    float distance(const glm::vec3& p) const {
        return glm::dot(n, p) + d;
    }
};

// ====================== Frustum ======================
class Frustum {
public:
    // Order: left, right, bottom, top, near, far
    std::array<Plane, 6> planes;

    // 从 VP 矩阵提取 6 个平面（OpenGL clip space 约定）
    void updateFromMatrix(const glm::mat4& vp) {
        // glm 是列主序：vp[col][row]
        // 行向量：row0..row3
        glm::vec4 row0 = glm::row(vp, 0);
        glm::vec4 row1 = glm::row(vp, 1);
        glm::vec4 row2 = glm::row(vp, 2);
        glm::vec4 row3 = glm::row(vp, 3);

        // Left:  row3 + row0
        setPlane(0, row3 + row0);
        // Right: row3 - row0
        setPlane(1, row3 - row0);
        // Bottom: row3 + row1
        setPlane(2, row3 + row1);
        // Top:    row3 - row1
        setPlane(3, row3 - row1);
        // Near:   row3 + row2
        setPlane(4, row3 + row2);
        // Far:    row3 - row2
        setPlane(5, row3 - row2);

        for (auto& p : planes) p.normalize();
    }

    // AABB vs Frustum（“积极顶点”测试法，快速且常用）
    bool intersects(const AABB& box) const {
        for (const auto& p : planes) {
            glm::vec3 positive = box.min;
            if (p.n.x >= 0) positive.x = box.max.x;
            if (p.n.y >= 0) positive.y = box.max.y;
            if (p.n.z >= 0) positive.z = box.max.z;

            // 若积极顶点都在平面外侧，则整个 AABB 在外侧
            if (p.distance(positive) < 0.0f) {
                return false;
            }
        }
        return true;
    }

private:
    void setPlane(int idx, const glm::vec4& eq) {
        planes[idx].n = glm::vec3(eq.x, eq.y, eq.z);
        planes[idx].d = eq.w;
    }
};
