#ifndef COLLISION_HPP
#define COLLISION_HPP

#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include "include/terrain/terrain.hpp"

struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;
};

class CollisionSystem {
public:
    // 地图边界参数（在 main.cpp 中初始化）
    inline static float mapMinX = -1024.0f;
    inline static float mapMaxX =  1024.0f;
    inline static float mapMinZ = -1024.0f;
    inline static float mapMaxZ =  1024.0f;

    // 湖泊参数（在 main.cpp 中初始化）
    inline static float lakeCX = 80.0f;
    inline static float lakeCZ = 80.0f;
    inline static float lakeRadius = 50.0f;

    // 获取单例障碍物列表
    static std::vector<BoundingBox>& getObstacles() {
        static std::vector<BoundingBox> obstacles;
        return obstacles;
    }

    // 每一帧开始前清空旧的动态碰撞盒，然后重新添加边界墙壁和湖泊围墙
    static void clearObstacles() {
        getObstacles().clear();
        addBoundaryWalls();
        addLakeCollision();
    }

    // 四面围墙包围地图
    static void addBoundaryWalls() {
        float wallThickness = 5.0f;   // 墙壁厚度
        float wallHeight = 200.0f;    // 墙壁高度（足够高，车不可能飞过）

        // 北墙 (Z = mapMaxZ)
        getObstacles().push_back({ {mapMinX - wallThickness, -100.0f, mapMaxZ},
                                    {mapMaxX + wallThickness, wallHeight, mapMaxZ + wallThickness} });
        // 南墙 (Z = mapMinZ)
        getObstacles().push_back({ {mapMinX - wallThickness, -100.0f, mapMinZ - wallThickness},
                                    {mapMaxX + wallThickness, wallHeight, mapMinZ} });
        // 东墙 (X = mapMaxX)
        getObstacles().push_back({ {mapMaxX, -100.0f, mapMinZ - wallThickness},
                                    {mapMaxX + wallThickness, wallHeight, mapMaxZ + wallThickness} });
        // 西墙 (X = mapMinX)
        getObstacles().push_back({ {mapMinX - wallThickness, -100.0f, mapMinZ - wallThickness},
                                    {mapMinX, wallHeight, mapMaxZ + wallThickness} });
    }

    // 湖泊碰撞围墙：用16段弧形AABB拼成近似圆形
    static void addLakeCollision() {
        int segments = 16;
        float wallThickness = 2.0f;
        float wallHeight = 200.0f;

        for (int i = 0; i < segments; ++i) {
            float angle0 = 2.0f * 3.14159265f * i / segments;
            float angle1 = 2.0f * 3.14159265f * (i + 1) / segments;

            float x0 = lakeCX + lakeRadius * std::cos(angle0);
            float z0 = lakeCZ + lakeRadius * std::sin(angle0);
            float x1 = lakeCX + lakeRadius * std::cos(angle1);
            float z1 = lakeCZ + lakeRadius * std::sin(angle1);

            float minX = std::min(x0, x1) - wallThickness;
            float maxX = std::max(x0, x1) + wallThickness;
            float minZ = std::min(z0, z1) - wallThickness;
            float maxZ = std::max(z0, z1) + wallThickness;

            getObstacles().push_back({
                {minX, -100.0f, minZ},
                {maxX, wallHeight, maxZ}
            });
        }
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