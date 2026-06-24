#ifndef CAR_HPP
#define CAR_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "include/terrain/terrain.hpp"

class Car {
public:
    glm::vec3 Position;
    float Heading;      // 赛车朝向
    float Speed;
    float SteerAngle;   // 前轮转向角
    float WheelRotation;// 车轮自转角

    Car(glm::vec3 startPos) : Position(startPos), Heading(0.0f), Speed(0.0f), SteerAngle(0.0f), WheelRotation(0.0f) {}

    void Update(float deltaTime, float terrainHeight, bool w, bool s, bool a, bool d) {
        // 1. 处理动力
        if (w) Speed += 10.0f * deltaTime;
        else if (s) Speed -= 10.0f * deltaTime;
        else Speed *= 0.98f; // 阻尼

        // 2. 处理转向 (只有在移动时才能转向)
        float steerSpeed = 40.0f * deltaTime;
        if (a) SteerAngle = glm::clamp(SteerAngle + steerSpeed, -30.0f, 30.0f);
        else if (d) SteerAngle = glm::clamp(SteerAngle - steerSpeed, -30.0f, 30.0f);
        else SteerAngle *= 0.9f;

        // 3. 更新位置和朝向
        if (glm::abs(Speed) > 0.1f) {
            Heading += SteerAngle * (Speed / 50.0f) * deltaTime;
        }

        //Position.x += glm::sin(glm::radians(Heading)) * Speed * deltaTime;
        //Position.z += glm::cos(glm::radians(Heading)) * Speed * deltaTime;
        //Position.y = terrainHeight; // 贴地

        // 4. 车轮自转
        WheelRotation += Speed * deltaTime * 50.0f;
    }

    glm::mat4 Car::GetModelMatrix(const Terrain& terrain) {
        // 1. 获取当前位置的法线
        glm::vec3 normal = terrain.getNormalWorld(Position.x, Position.z);

        // 2. 计算赛车的水平朝向向量
        glm::vec3 forward = glm::vec3(sin(glm::radians(Heading)), 0, cos(glm::radians(Heading)));

        // 3. 计算贴合地面的右向量 (Right = Forward x Normal)
        glm::vec3 right = glm::normalize(glm::cross(forward, normal));

        // 4. 计算贴合地面
        glm::vec3 actualForward = glm::normalize(glm::cross(normal, right));

        // 5. 构建旋转矩阵
        glm::mat4 rotation = glm::mat4(1.0f);
        rotation[0] = glm::vec4(right, 0.0f);
        rotation[1] = glm::vec4(normal, 0.0f);
        rotation[2] = glm::vec4(actualForward, 0.0f);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        model = model * rotation; // 应用贴地旋转
        return model;
    }
};

#endif