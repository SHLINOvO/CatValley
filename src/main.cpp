#include <iostream>
#include <filesystem> 
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"
#include "include/camera.hpp"
#include "include/shader.hpp"
#include "include/model.hpp"
#include "include/skybox.hpp"
#include "include/terrain/terrain.hpp"
#include "include/ani.hpp"
#include "include/collision.hpp"
#include "include/vegetation/vegetation.hpp"
#include "include/water/waterMesh.hpp"

// ———————— 全局变量 ————————
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
bool cursorLocked = false;
float eyeHeight = 10.0f;

Camera camera(glm::vec3(0.0f, eyeHeight, 5.0f));
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

Car myCar(glm::vec3(2.0f, 0.0f, -5.0f));
bool driveMode = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool topView = false;
glm::vec3 originalCameraPos;
glm::vec3 originalCameraFront;
glm::vec3 originalCameraUp;
float originalZoom;

// ———————— 回调函数 ————————
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!cursorLocked) {
        lastX = static_cast<float>(xposIn);
        lastY = static_cast<float>(yposIn);
        return;
    }
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;
    if (!topView) camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.Position += camera.Front * static_cast<float>(yoffset) * 2.0f;
}

void processInput(GLFWwindow* window, Car& myCar, Terrain& terrain) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool altPressed = false;
    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
        if (!altPressed) {
            cursorLocked = !cursorLocked;
            glfwSetInputMode(window, GLFW_CURSOR, cursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            firstMouse = true;
            altPressed = true;
        }
    } else { altPressed = false; }

    static bool cPressed = false;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (!cPressed) { driveMode = !driveMode; cPressed = true; if (driveMode) firstMouse = true; }
    } else { cPressed = false; }

    if (driveMode) {
        bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        float groundY = terrain.getHeightWorld(myCar.Position.x, myCar.Position.z);
        myCar.Update(deltaTime, groundY, w, s, a, d);
    } else {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
        float speed = camera.MovementSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) eyeHeight += speed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) eyeHeight -= speed;
    }
}

// ———————— 主函数 ————————
int main() {
    // 赛车初始朝向湖泊（湖心 (80,80)，车在 (2,-5)），atan2(78,85) ≈ 43°
    myCar.Heading = 43.0f;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Cat Valley", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "Failed to initialize GLEW" << std::endl; return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ==================== Shadow Map FBO ====================
    GLuint shadowFBO;
    GLuint shadowMapTexture;
    glGenFramebuffers(1, &shadowFBO);
    glGenTextures(1, &shadowMapTexture);

    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ==================== Shaders ====================
    Shader ourShader((std::string(SHADERS_FOLDER) + "model.vs").c_str(), (std::string(SHADERS_FOLDER) + "model.fs").c_str());
    Shader animShader((std::string(SHADERS_FOLDER) + "ani.vs").c_str(), (std::string(SHADERS_FOLDER) + "model.fs").c_str());
    Shader treeShader((std::string(SHADERS_FOLDER) + "tree.vs").c_str(), (std::string(SHADERS_FOLDER) + "tree.fs").c_str());
    Shader shadowShader((std::string(SHADERS_FOLDER) + "shadow_depth.vs").c_str(), (std::string(SHADERS_FOLDER) + "shadow_depth.fs").c_str());

    // ==================== Light Space Matrix ====================
    // Unified light direction: from surface toward sun normalize(vec3(0.3, 1.0, 0.4))
    glm::vec3 lightDirToSun = glm::normalize(glm::vec3(0.3f, 1.0f, 0.4f));
    float shadowOrthoSize = 120.0f;
    glm::mat4 lightProjection = glm::ortho(-shadowOrthoSize, shadowOrthoSize, -shadowOrthoSize, shadowOrthoSize, 1.0f, 500.0f);
    // lightSpaceMatrix is computed per-frame inside the render loop to follow the camera

    // ==================== Models & Scene ====================
    AniModel catModel1((std::string(ASSETS_FOLDER) + "munchkin_cat2/scene.gltf").c_str(), (std::string(ASSETS_FOLDER) + "munchkin_cat2/").c_str());
    AniModel catModel2((std::string(ASSETS_FOLDER) + "munchkin_cat/scene.gltf").c_str(), (std::string(ASSETS_FOLDER) + "munchkin_cat/").c_str());
    Model mclaren((std::string(ASSETS_FOLDER) + "car/f1_2025_mclaren_mcl39.glb").c_str(), (std::string(ASSETS_FOLDER) + "car/").c_str());

    // --- 猫数据结构定义 ---
    enum CatState { IDLE, WALK, RUN };
    struct CatInstance {
        AniModel* model;
        glm::vec3 pos;
        float yaw;
        float scale;
        CatState state;
        float moveSpeed;
        float moveYaw;
        float stateTimer;  // 当前状态剩余时间
    };
    std::vector<CatInstance> cats;  // 猫列表（terrain 后填充）

    std::vector<std::string> faces = {
        ASSETS_FOLDER "skybox/right.jpg", ASSETS_FOLDER "skybox/left.jpg",
        ASSETS_FOLDER "skybox/top.jpg", ASSETS_FOLDER "skybox/bottom.jpg",
        ASSETS_FOLDER "skybox/front.jpg", ASSETS_FOLDER "skybox/back.jpg"
    };
    Skybox skybox(faces);

    Terrain terrain(ASSETS_FOLDER "terrain/heightmap_2049.png", 1800.0f, 64, 64, 33, 1.0f);
    terrain.setLODDistances(200.0f, 600.0f, 1400.0f);

    // --- 挖湖凹陷 ---
    const float LAKE_CX = 80.0f;
    const float LAKE_CZ = 80.0f;
    const float LAKE_RADIUS = 50.0f;
    const float LAKE_DEPTH = 35.0f;    // 世界单位降低量
    const float LAKE_BLEND = 10.0f;    // 边缘渐变宽度

    // 先记录原始地面高度（用于水面水位），再挖湖
    float originalGroundH = terrain.getHeightWorld(LAKE_CX, LAKE_CZ);

    terrain.heightmap.carveLake(LAKE_CX, LAKE_CZ, LAKE_RADIUS, LAKE_DEPTH, LAKE_BLEND, 1.0f);

    // carveLake 修改了 heightmap 数据，必须重建受影响的 terrain chunks GPU 缓冲
    terrain.rebuildLakeChunks(LAKE_CX, LAKE_CZ, LAKE_RADIUS + LAKE_BLEND);

    // 水面高度：接近原始地面高度，略低2个单位（露出岸边）
    float waterY = originalGroundH - 2.0f;

    // --- 水面网格 ---
    WaterMesh waterMesh(LAKE_CX, LAKE_CZ, LAKE_RADIUS, waterY, 64);

    // --- 水面着色器 ---
    Shader waterShader((std::string(SHADERS_FOLDER) + "water.vs").c_str(), (std::string(SHADERS_FOLDER) + "water.fs").c_str());

    // --- 反射 FBO ---
    GLuint reflectFBO, reflectColorTex, reflectDepthTex;
    glGenFramebuffers(1, &reflectFBO);
    glGenTextures(1, &reflectColorTex);
    glGenTextures(1, &reflectDepthTex);

    glBindTexture(GL_TEXTURE_2D, reflectColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, reflectDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, reflectFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflectColorTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, reflectDepthTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    auto vegetation = CreateDefaultVegetationManager(terrain, 20260120, -1000.0f, 1000.0f, -1000.0f, 1000.0f, { 10.0f, 10.0f }, 10.0f);

    // 删除湖泊区域内的所有植被实例（树不应长在水里）
    vegetation->removeInstancesInCircle(LAKE_CX, LAKE_CZ, LAKE_RADIUS);

    // --- 地图边界碰撞墙 ---
    // heightmap 2049×2049, gridScale=1.0 → 世界坐标 [-1024, 1024]
    CollisionSystem::mapMinX = -1024.0f;
    CollisionSystem::mapMaxX =  1024.0f;
    CollisionSystem::mapMinZ = -1024.0f;
    CollisionSystem::mapMaxZ =  1024.0f;
    CollisionSystem::lakeCX = LAKE_CX;
    CollisionSystem::lakeCZ = LAKE_CZ;
    CollisionSystem::lakeRadius = LAKE_RADIUS;

    // --- 大猫：随机位置、朝向、分散放置（全部用大猫模型） ---
    std::mt19937 catRng(42);
    std::uniform_real_distribution<float> catPosDist(-900.0f, 900.0f);
    std::uniform_real_distribution<float> catYawDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> catScaleDist1(25.0f, 35.0f);
    std::uniform_real_distribution<float> catIdleDist(0.0f, 1.0f);  // 状态随机

    const int NUM_BIG_CATS = 10;
    const float CAT_MIN_SPACING = 40.0f;
    const float CAT_MIN_DIST_FROM_CAR = 30.0f;

    std::vector<glm::vec2> placedPositions;
    int catsPlaced = 0;
    int catPlaceTries = 0;
    const int MAX_PLACE_TRIES = 1000;

    while (catsPlaced < NUM_BIG_CATS && catPlaceTries < MAX_PLACE_TRIES) {
        catPlaceTries++;
        float px = catPosDist(catRng);
        float pz = catPosDist(catRng);

        // 不在车起始位置附近放猫
        glm::vec2 carStartXZ(2.0f, -5.0f);
        if (glm::distance(glm::vec2(px, pz), carStartXZ) < CAT_MIN_DIST_FROM_CAR) continue;

        // 不在花丛区域内放猫（留给小猫）
        glm::vec2 flowerBedCenter(10.0f, 10.0f);
        if (glm::distance(glm::vec2(px, pz), flowerBedCenter) < 12.0f) continue;

        // 不在湖泊区域内放猫
        glm::vec2 lakeCenter(LAKE_CX, LAKE_CZ);
        if (glm::distance(glm::vec2(px, pz), lakeCenter) < LAKE_RADIUS + 5.0f) continue;

        // 猫之间最小间距
        bool tooClose = false;
        for (const auto& pp : placedPositions) {
            if (glm::distance(glm::vec2(px, pz), pp) < CAT_MIN_SPACING) { tooClose = true; break; }
        }
        if (tooClose) continue;

        float py = terrain.getHeightWorld(px, pz);
        float catScale = catScaleDist1(catRng);
        float catYaw = catYawDist(catRng);

        cats.push_back({
            &catModel1,
            glm::vec3(px, py, pz),
            catYaw, catScale, IDLE, 0.0f, catYaw, 0.0f
        });
        placedPositions.push_back(glm::vec2(px, pz));
        catsPlaced++;
    }

    // --- 小猫：放在花丛中 ---
    glm::vec2 flowerBed(10.0f, 10.0f);
    float smallCatX = flowerBed.x + 1.0f;   // 花丛中心偏移一点，免得正中间太突兀
    float smallCatZ = flowerBed.y + 2.0f;
    float smallCatY = terrain.getHeightWorld(smallCatX, smallCatZ);
    cats.push_back({
        &catModel2,
        glm::vec3(smallCatX, smallCatY, smallCatZ),
        catYawDist(catRng), 5.0f, IDLE, 0.0f, 0.0f, 0.0f
    });

    // ———————— 渲染循环 ————————
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        CollisionSystem::clearObstacles();

        // --- Cat logic (all cats) ---
        glm::vec2 carXZ(myCar.Position.x, myCar.Position.z);
        for (auto& cat : cats) {
            cat.pos.y = terrain.getHeightWorld(cat.pos.x, cat.pos.z);

            // 随机漫步：计时器到期后随机切换状态（只有 IDLE 和 RUN）
            cat.stateTimer -= deltaTime;
            if (cat.stateTimer <= 0.0f) {
                // 随机选下一个状态：50% IDLE, 50% RUN
                float r = catIdleDist(catRng);
                if (r < 0.50f) { cat.state = IDLE; cat.moveSpeed = 0.0f; }
                else { cat.state = RUN; cat.moveSpeed = 6.0f; cat.moveYaw = catYawDist(catRng); }
                // 随机持续时间：IDLE 2~5秒, RUN 1~3秒
                switch (cat.state) {
                case IDLE:  cat.stateTimer = 2.0f + 3.0f * catIdleDist(catRng); break;
                case RUN:   cat.stateTimer = 1.0f + 2.0f * catIdleDist(catRng); break;
                }
            }

            if (cat.moveSpeed > 0.0f) {
                glm::vec3 moveDir = glm::vec3(
                    glm::sin(glm::radians(cat.moveYaw)),
                    0.0f,
                    glm::cos(glm::radians(cat.moveYaw))
                );
                cat.pos += moveDir * cat.moveSpeed * deltaTime;
                cat.pos.y = terrain.getHeightWorld(cat.pos.x, cat.pos.z);
                cat.yaw = cat.moveYaw;
            }

            // 碰撞 AABB
            BoundingBox catAABB;
            float catHitBoxSize = 2.5f;
            catAABB.min = cat.pos - glm::vec3(catHitBoxSize, 0.0f, catHitBoxSize);
            catAABB.max = cat.pos + glm::vec3(catHitBoxSize, catHitBoxSize * 1.5f, catHitBoxSize);
            CollisionSystem::getObstacles().push_back(catAABB);
        }

        // --- Tree trunk collision (shrub & groundcover pass through) ---
        for (const auto& tree : vegetation->getTreeCollisionInfos()) {
            BoundingBox trunkAABB;
            trunkAABB.min = tree.pos - glm::vec3(tree.trunkRadius, 0.0f, tree.trunkRadius);
            trunkAABB.max = tree.pos + glm::vec3(tree.trunkRadius, tree.trunkHeight, tree.trunkRadius);
            CollisionSystem::getObstacles().push_back(trunkAABB);
        }

        // --- Car physics ---
        processInput(window, myCar, terrain);
        CollisionSystem::updatePositionWithPhysics(myCar.Position, myCar.Heading, myCar.Speed, deltaTime, terrain);

        if (driveMode) {
            glm::vec3 offset;
            offset.x = -glm::sin(glm::radians(myCar.Heading)) * 4.5f;
            offset.z = -glm::cos(glm::radians(myCar.Heading)) * 4.5f;
            offset.y = 2.5f;
            glm::vec3 targetCameraPos = myCar.Position + offset;
            glm::vec3 lookTarget = myCar.Position + glm::vec3(0.0f, 3.0f, 0.0f);
            camera.Position = glm::mix(camera.Position, targetCameraPos, glm::clamp(5.0f * deltaTime, 0.0f, 1.0f));
            glm::vec3 lookDirection = glm::normalize(lookTarget - camera.Position);
            camera.Front = lookDirection;
            camera.Right = glm::normalize(glm::cross(camera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
            camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
            camera.Yaw = glm::degrees(atan2(camera.Front.z, camera.Front.x));
            camera.Pitch = glm::degrees(asin(camera.Front.y));
        } else if (!topView) {
            camera.Position.y = terrain.getHeightWorld(camera.Position.x, camera.Position.z) + eyeHeight;
        }

        // ==================== Compute model matrices ====================
        // All cats: compute model matrix and set animation state
        std::vector<glm::mat4> catModelMats(cats.size());
        for (size_t i = 0; i < cats.size(); i++) {
            auto& cat = cats[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), cat.pos);
            M = glm::rotate(M, glm::radians(cat.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            M = glm::scale(M, glm::vec3(cat.scale));
            catModelMats[i] = M;
        }

        glm::vec3 normal = terrain.getNormalWorld(myCar.Position.x, myCar.Position.z);
        glm::vec3 forward = glm::vec3(glm::sin(glm::radians(myCar.Heading)), 0, glm::cos(glm::radians(myCar.Heading)));
        glm::vec3 right = glm::normalize(glm::cross(forward, normal));
        glm::vec3 actualForward = glm::normalize(glm::cross(normal, right));
        glm::mat4 rotation = glm::mat4(1.0f);
        rotation[0] = glm::vec4(right, 0.0f);
        rotation[1] = glm::vec4(normal, 0.0f);
        rotation[2] = glm::vec4(actualForward, 0.0f);
        glm::vec3 adjustedPose = myCar.Position + 0.50f;
        glm::mat4 modelCar = glm::translate(glm::mat4(1.0f), adjustedPose) * rotation;

        // ==================== Per-frame Light Space Matrix (follows camera) ====================
        glm::vec3 shadowCenter = driveMode ? myCar.Position : camera.Position;
        glm::vec3 lightPos = shadowCenter + lightDirToSun * 200.0f;
        glm::mat4 lightView = glm::lookAt(lightPos, shadowCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // ======================== SHADOW PASS ========================
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glDisable(GL_BLEND);

    shadowShader.use();
    shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    // Polygon offset: push shadow-map depth back to eliminate self-shadowing acne
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    // Cat shadows — 每只猫单独设置 externalState 再绘制
        for (size_t i = 0; i < cats.size(); i++) {
            cats[i].model->externalState = (int)cats[i].state;
            cats[i].model->DrawShadow(shadowShader, catModelMats[i], currentFrame);
        }

        // Car shadow
        mclaren.Draw(shadowShader, modelCar);

        // Terrain shadow
        terrain.renderShadow(shadowShader, lightSpaceMatrix, shadowCenter);

        // Vegetation shadow
        vegetation->renderShadow(terrain, shadowShader, lightSpaceMatrix);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_BLEND);

    // ======================== REFLECTION PASS ========================
        // 镜面摄像机：沿水面 Y 平面翻转
        glm::vec3 reflCamPos = camera.Position;
        reflCamPos.y = 2.0f * waterY - camera.Position.y;  // 镜像 Y

        glm::vec3 reflCamFront = camera.Front;
        reflCamFront.y = -camera.Front.y;  // 镜像 Front 的 Y
        reflCamFront = glm::normalize(reflCamFront);

        glm::vec3 reflCamUp = camera.Up;
        reflCamUp.y = -camera.Up.y;  // 镜像 Up 的 Y
        reflCamUp = glm::normalize(reflCamUp);

        // 构建镜面 view matrix
        glm::mat4 reflView = glm::lookAt(reflCamPos, reflCamPos + reflCamFront, reflCamUp);

        // 渲染到反射 FBO
        glBindFramebuffer(GL_FRAMEBUFFER, reflectFBO);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);  // 天蓝色（天空反射颜色）
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_CLIP_DISTANCE0);  // 裁剪水面以上内容
        glDisable(GL_BLEND);

        glm::mat4 reflProjection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 2.0f, 5000.0f);

        // 绑定 shadow map 到反射 Pass
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, shadowMapTexture);

        // 反射 Pass: 设置 clipPlaneY = waterY（裁剪水面以上的世界）
        float clipY_refl = waterY + 0.1f;  // 轻微偏移避免自裁剪

        // Terrain (reflection)
        terrain.terrainShader.use();
        terrain.terrainShader.setMat4("model", glm::mat4(1.0f));
        terrain.terrainShader.setMat4("view", reflView);
        terrain.terrainShader.setMat4("projection", reflProjection);
        terrain.terrainShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        terrain.terrainShader.setInt("shadowMap", 10);
        terrain.terrainShader.setVec3("lightDir", glm::normalize(glm::vec3(-0.3f, -1.0f, -0.4f)));
        terrain.terrainShader.setVec3("lightColor", glm::vec3(1.2f, 1.15f, 1.05f));
        terrain.terrainShader.setVec3("viewPos", reflCamPos);
        terrain.terrainShader.setFloat("clipPlaneY", clipY_refl);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, terrain.grassLowTex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, terrain.grassHighTex);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, terrain.noiseTex);
        terrain.terrainSystem.Draw(terrain.terrainShader, glm::mat4(1.0f), reflView, reflProjection, reflCamPos);

        // Car (reflection)
        ourShader.use();
        ourShader.setMat4("projection", reflProjection);
        ourShader.setMat4("view", reflView);
        ourShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        ourShader.setVec3("viewPos", reflCamPos);
        ourShader.setVec3("lightDir", lightDirToSun);
        ourShader.setVec3("lightColor", glm::vec3(4.0f, 3.6f, 3.2f));
        ourShader.setFloat("metallic",   0.7f);
        ourShader.setFloat("roughness",  0.35f);
        ourShader.setFloat("exposure",   1.8f);
        ourShader.setInt("shadowMap", 10);
        ourShader.setFloat("clipPlaneY", clipY_refl);
        mclaren.DrawCar(ourShader, modelCar, myCar);

        // Cats (reflection)
        animShader.use();
        animShader.setMat4("projection", reflProjection);
        animShader.setMat4("view", reflView);
        animShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        animShader.setVec3("viewPos", reflCamPos);
        animShader.setVec3("lightDir", lightDirToSun);
        animShader.setVec3("lightColor", glm::vec3(4.0f, 3.6f, 3.2f));
        animShader.setFloat("metallic",   0.0f);
        animShader.setFloat("roughness",  0.6f);
        animShader.setFloat("exposure",   1.8f);
        animShader.setInt("shadowMap", 10);
        animShader.setFloat("clipPlaneY", clipY_refl);
        for (size_t i = 0; i < cats.size(); i++) {
            cats[i].model->externalState = (int)cats[i].state;
            animShader.use();
            cats[i].model->UpdateAndDraw(animShader, currentFrame, catModelMats[i]);
        }

        // Vegetation (reflection)
        treeShader.use();
        treeShader.setMat4("projection", reflProjection);
        treeShader.setMat4("view", reflView);
        treeShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        treeShader.setVec3("viewPos", reflCamPos);
        treeShader.setVec3("lightDir", lightDirToSun);
        treeShader.setVec3("lightColor", glm::vec3(4.0f, 3.6f, 3.2f));
        treeShader.setFloat("metallic",   0.0f);
        treeShader.setFloat("roughness",  0.7f);
        treeShader.setFloat("exposure",   1.8f);
        treeShader.setInt("shadowMap", 10);
        treeShader.setFloat("clipPlaneY", clipY_refl);
        vegetation->renderWithShadow(terrain, treeShader, reflView, reflProjection, reflCamPos, lightSpaceMatrix, 10);

        // Skybox (reflection)
        skybox.draw(reflView, reflProjection);

        glDisable(GL_CLIP_DISTANCE0);
        glEnable(GL_BLEND);

        // ======================== NORMAL RENDERING PASS ========================
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int currentWidth, currentHeight;
        glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
        glViewport(0, 0, currentWidth, currentHeight);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)currentWidth / (float)currentHeight, 2.0f, 5000.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // Bind shadow map to texture unit 10
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, shadowMapTexture);

        // Shared PBR lighting params
        glm::vec3 sunDir = lightDirToSun; // normalize(vec3(0.3, 1.0, 0.4)), surface->sun

        // Normal pass: clipPlaneY 设极大值，不裁剪
        float clipY_normal = 99999.0f;

        // ourShader setup (car: metallic, fairly smooth)
        ourShader.use();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        ourShader.setVec3("viewPos", camera.Position);
        ourShader.setVec3("lightDir", sunDir);
        ourShader.setVec3("lightColor", glm::vec3(4.0f, 3.6f, 3.2f));  // outdoor sunlight radiance (PBR needs ~3-5)
        ourShader.setFloat("metallic",   0.7f);
        ourShader.setFloat("roughness",  0.35f);
        ourShader.setFloat("exposure",   1.8f);  // PBR exposure control
        ourShader.setInt("shadowMap", 10);
        ourShader.setFloat("clipPlaneY", clipY_normal);

        // animShader setup (cats: non-metallic, medium roughness)
        animShader.use();
        animShader.setMat4("projection", projection);
        animShader.setMat4("view", view);
        animShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        animShader.setVec3("viewPos", camera.Position);
        animShader.setVec3("lightDir", sunDir);
        animShader.setVec3("lightColor", glm::vec3(4.0f, 3.6f, 3.2f));
        animShader.setFloat("metallic",   0.0f);
        animShader.setFloat("roughness",  0.6f);
        animShader.setFloat("exposure",   1.8f);
        animShader.setInt("shadowMap", 10);
        animShader.setFloat("clipPlaneY", clipY_normal);

        // Draw all cats — 每只猫单独设置 externalState 再绘制
        for (size_t i = 0; i < cats.size(); i++) {
            cats[i].model->externalState = (int)cats[i].state;
            animShader.use();
            cats[i].model->UpdateAndDraw(animShader, currentFrame, catModelMats[i]);
        }

        // Draw car
        ourShader.use();
        mclaren.DrawCar(ourShader, modelCar, myCar);

        // Draw terrain with shadow
        terrain.renderWithShadow(view, projection, camera.Position, lightSpaceMatrix, 10);
        // 给 terrain shader 设 clipPlaneY（renderWithShadow 内部已经 use() 过了）
        terrain.terrainShader.setFloat("clipPlaneY", clipY_normal);

        // Draw vegetation with shadow
        vegetation->renderWithShadow(terrain, treeShader, view, projection, camera.Position, lightSpaceMatrix, 10);
        treeShader.setFloat("clipPlaneY", clipY_normal);

        // Draw skybox
        skybox.draw(view, projection);

        // ======================== WATER PASS ========================
        // 水面半透明渲染（最后绘制，alpha 混合）
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);  // 水面不写深度，避免遮挡后方透明物体

        waterShader.use();
        waterShader.setMat4("model", glm::mat4(1.0f));
        waterShader.setMat4("view", view);
        waterShader.setMat4("projection", projection);
        waterShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        waterShader.setVec3("viewPos", camera.Position);
        waterShader.setVec3("lightDir", sunDir);
        waterShader.setVec3("lightColor", glm::vec3(4.0f, 3.6f, 3.2f));
        waterShader.setFloat("time", currentFrame);
        waterShader.setFloat("waterHeight", waterY);
        waterShader.setFloat("clipPlaneY", clipY_normal);
        waterShader.setInt("shadowMap", 10);

        // 绑定反射纹理到 texture unit 5
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, reflectColorTex);
        waterShader.setInt("reflectionTexture", 5);

        waterMesh.Draw();

        glDepthMask(GL_TRUE);  // 恢复深度写入

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &shadowFBO);
    glDeleteTextures(1, &shadowMapTexture);
    glDeleteFramebuffers(1, &reflectFBO);
    glDeleteTextures(1, &reflectColorTex);
    glDeleteTextures(1, &reflectDepthTex);
    glfwTerminate();
    return 0;
}
