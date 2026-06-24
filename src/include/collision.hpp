#ifndef COLLISION_HPP
#define COLLISION_HPP

#include <glm/glm.hpp>
#include <vector>
#include "include/terrain/terrain.hpp"

struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;
};

class CollisionSystem {
public:
    // 获取单例障碍物列表
    static std::vector<BoundingBox>& getObstacles() {
        static std::vector<BoundingBox> obstacles;
        return obstacles;
    }

    // 每一帧开始前清空旧的动态碰撞盒
    static void clearObstacles() {
        getObstacles().clear();
    }

    // 核心碰撞检测：增加一个 radius 让赛车有一定体积
    static bool checkCollision(const glm::vec3& pos, float radius) {
        for (const auto& box : getObstacles()) {
            if (pos.x + radius > box.min.x && pos.x - radius < box.max.x &&
                pos.y + radius > box.min.y && pos.y - radius < box.max.y &&
                pos.z + radius > box.min.z && pos.z - radius < box.max.z) {
                return true;
            }
        }
        return false;
    }

    static void updatePositionWithPhysics(glm::vec3& pos, float heading, float& speed, float deltaTime, const Terrain& terrain) {
        // 限制 deltaTime 防止大跨步穿墙
        float frameTime = glm::min(deltaTime, 0.033f);

        // 1. 预测
        float moveDist = speed * frameTime;
        glm::vec3 direction = glm::vec3(glm::sin(glm::radians(heading)), 0, glm::cos(glm::radians(heading)));
        glm::vec3 nextPos = pos + direction * moveDist;
        nextPos.y = terrain.getHeightWorld(nextPos.x, nextPos.z);

        // 2. 改进的碰撞检测
        glm::vec3 checkCenter = nextPos + glm::vec3(0.0f, 1.0f, 0.0f);
        float carRadius = 1.2f;

        if (checkCollision(checkCenter, carRadius)) {
            // 发生碰撞时，除了减速，还要给一个微小的反弹位移，防止卡死在物体内部
            speed = -speed * 0.2f;
            pos -= direction * 0.1f; // 强制回退一点点，打破“卡住”的循环
        }
        else {
            // 使用平滑过渡更新位置，减少瞬移感
            pos = glm::mix(pos, nextPos, 0.9f);
        }
    }

    // 相机防入地
    static glm::vec3 resolveCameraCollision(const glm::vec3& targetCamPos, const Terrain& terrain) {
        float h = terrain.getHeightWorld(targetCamPos.x, targetCamPos.z);
        glm::vec3 finalPos = targetCamPos;
        if (finalPos.y < h + 0.8f) finalPos.y = h + 0.8f;
        return finalPos;
    }
};

#endif