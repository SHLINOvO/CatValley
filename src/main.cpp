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

// ———————— 全局变量 ————————
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
bool cursorLocked = false;   // 默认锁定鼠标并控制视角
float eyeHeight = 10.0f;    // 初始相机高度

Camera camera(glm::vec3(0.0f, eyeHeight, 5.0f));
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

Car myCar(glm::vec3(2.0f, 0.0f, -5.0f));
bool driveMode = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool topView = false;   //是否处于俯视模式
glm::vec3 originalCameraPos;    //保存原始相机位置
glm::vec3 originalCameraFront;  //保存原始相机朝向
glm::vec3 originalCameraUp;     //保存原始相机上方向
float originalZoom;          //保存原始FOV

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

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    if(!topView)
    camera.ProcessMouseMovement(xoffset, yoffset);
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
    }
    else { altPressed = false; }

    // --- 3. WASDQE 移动控制 ---
    // WASD 移动
    // QE 修改相对于地面的高度
    // 按 C 键：手动切换【自由探索】和【驾驶模式】
    static bool cPressed = false;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (!cPressed) {
            driveMode = !driveMode;
            cPressed = true;
            if (driveMode) firstMouse = true;
        }
    }
    else { cPressed = false; }

    if (driveMode) {
        // --- 模式 A：驾驶赛车 ---
        bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

        float groundY = terrain.getHeightWorld(myCar.Position.x, myCar.Position.z);
        myCar.Update(deltaTime, groundY, w, s, a, d);
    }
    else {
        // --- 模式 B：自由相机 ---
        // 只有在这里，WASD 才控制相机移动
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
    // 初始化 GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Model Viewer (Assimp + GLEW)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // 初始化 GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // ———————— 初始化从这开始 ———————— 
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 创建着色器（需准备 .vs 和 .fs 文件）
    Shader ourShader((std::string(SHADERS_FOLDER) + "model.vs").c_str(), (std::string(SHADERS_FOLDER) + "model.fs").c_str());
    Shader animShader((std::string(SHADERS_FOLDER) + "ani.vs").c_str(), (std::string(SHADERS_FOLDER) + "model.fs").c_str());

    Shader treeShader(
        (std::string(SHADERS_FOLDER) + "tree.vs").c_str(),
        (std::string(SHADERS_FOLDER) + "tree.fs").c_str()
    );
    // 太阳方向（从太阳指向地面）
    glm::vec3 sunDir = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.4f));
    treeShader.use();
    treeShader.setVec3("light.position", sunDir);
    treeShader.setVec3("light.color", glm::vec3(1.0f));
    treeShader.setVec3("viewPos", camera.Position);

    AniModel cat1((std::string(ASSETS_FOLDER) + "munchkin_cat2/scene.gltf").c_str(), (std::string(ASSETS_FOLDER) + "munchkin_cat2/").c_str());
    glm::vec3 catPos(-2.0f, 0.0f, -5.0f);//猫的初始位置
    Model mclaren((std::string(ASSETS_FOLDER) + "car/f1_2025_mclaren_mcl39.glb").c_str(), (std::string(ASSETS_FOLDER) + "car/").c_str());
    Model cat((std::string(ASSETS_FOLDER)+"cat/Cat1/12221_Cat_v1_l3.obj").c_str(), (std::string(ASSETS_FOLDER) + "cat/Cat1/").c_str());
    AniModel cat2((std::string(ASSETS_FOLDER) + "munchkin_cat/scene.gltf").c_str(), (std::string(ASSETS_FOLDER) + "munchkin_cat/").c_str());
    glm::vec3 catPos2(10.0f, 0.0f, 10.0f);//猫的初始位置

    // 初始化、加载天空盒
    std::vector<std::string> faces = {
        ASSETS_FOLDER  "skybox/right.jpg",
        ASSETS_FOLDER  "skybox/left.jpg",
        ASSETS_FOLDER  "skybox/top.jpg",
        ASSETS_FOLDER  "skybox/bottom.jpg",
        ASSETS_FOLDER  "skybox/front.jpg",
        ASSETS_FOLDER  "skybox/back.jpg"
    };
    Skybox skybox(faces);

    // 初始化地形
    Terrain terrain(
        ASSETS_FOLDER "terrain/heightmap_2049.png",
        1800.0f,
        64, 64, 33, 1.0f
    );
    terrain.setLODDistances(200.0f, 600.0f, 1400.0f);

    auto vegetation = CreateDefaultVegetationManager(
        terrain,
        20260120,     // seed
        -1000.0f, 1000.0f,
        -1000.0f, 1000.0f,
        { 10.0f, 10.0f },   // flowerCenter
        10.0f   // flowerRadius
    );

    // ———————— 渲染循环 ————————
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- 1. 清空上一帧的所有碰撞盒 ---
        CollisionSystem::clearObstacles();

        // --- 2. 处理猫的逻辑 (移动猫并生成碰撞盒) ---
        catPos.y = terrain.getHeightWorld(catPos.x, catPos.z);

        // 状态机与距离计算
        glm::vec2 carXZ(myCar.Position.x, myCar.Position.z);
        glm::vec2 catXZ(catPos.x, catPos.z);
        float dist = glm::distance(carXZ, catXZ);
        float catMoveSpeed = 0.0f;
        enum CatState { IDLE, WALK, RUN }; //动作状态机
        CatState catState = IDLE;

        if (dist < 50.0f) { catState = RUN; catMoveSpeed = 10.0f; }
        else if (dist < 100.0f) { catState = WALK; catMoveSpeed = 2.5f; }

        // 更新猫的位置
        catPos.z += catMoveSpeed * deltaTime;
        catPos.y = terrain.getHeightWorld(catPos.x, catPos.z);

        // 猫注册到碰撞系统
        BoundingBox catAABB;
        float catHitBoxSize = 2.5f;
        catAABB.min = catPos - glm::vec3(catHitBoxSize, 0.0f, catHitBoxSize);
        catAABB.max = catPos + glm::vec3(catHitBoxSize, catHitBoxSize * 1.5f, catHitBoxSize);
        CollisionSystem::getObstacles().push_back(catAABB);
        
        // --- 3. 处理赛车的物理位移 (现在它能看到猫了) ---
        processInput(window, myCar, terrain);
        CollisionSystem::updatePositionWithPhysics(myCar.Position, myCar.Heading, myCar.Speed, deltaTime, terrain);

        if (driveMode) {
            // 计算理想偏移位置
            float distance = 4.5f;
            float height = 2.5f;

            glm::vec3 offset;
            offset.x = -glm::sin(glm::radians(myCar.Heading)) * distance;
            offset.z = -glm::cos(glm::radians(myCar.Heading)) * distance;
            offset.y = height;

            // 理想目标点
            glm::vec3 targetCameraPos = myCar.Position + offset;
            // 观察目标点
            glm::vec3 lookTarget = myCar.Position + glm::vec3(0.0f, 3.0f, 0.0f);

            float followSpeed = 5.0f;
            glm::vec3 nextCamPos = glm::mix(camera.Position, targetCameraPos, glm::clamp(followSpeed * deltaTime, 0.0f, 1.0f));
            camera.Position = nextCamPos;

            // 计算朝向
            glm::vec3 lookDirection = glm::normalize(lookTarget - camera.Position);
            camera.Front = lookDirection;
            camera.Right = glm::normalize(glm::cross(camera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
            camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));

            // 同步欧拉角
            camera.Yaw = glm::degrees(atan2(camera.Front.z, camera.Front.x));
            camera.Pitch = glm::degrees(asin(camera.Front.y));
        }
        else if (!topView) {
            // 获取当前位置的地形高度
            float groundY = terrain.getHeightWorld(camera.Position.x, camera.Position.z);
            camera.Position.y = groundY + eyeHeight;
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        // ------------模型可以共享的设置-------------
        // 设置 MVP
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 2.0f, 5000.0f); // 调整near和far，确保能看到远处的地形
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        // 光照
        ourShader.setVec3("viewPos", camera.Position);
        ourShader.setVec3("light.position", glm::vec3(0.0f, 1.0f, 10.0f));
        ourShader.setVec3("light.color", glm::vec3(1.0f, 1.0f, 1.0f));

        animShader.use();
        animShader.setMat4("projection", projection);
        animShader.setMat4("view", view);
        animShader.setVec3("viewPos", camera.Position);
        animShader.setVec3("light.position", glm::vec3(0.0f, 1.0f, 10.0f));
        animShader.setVec3("light.color", glm::vec3(1.0f, 1.0f, 1.0f));

        // 画猫
        glm::mat4 modelCat = glm::translate(glm::mat4(1.0f), catPos);
        modelCat = glm::scale(modelCat, glm::vec3(30.0f));
        animShader.use();
        cat1.externalState = (int)catState;
        cat1.UpdateAndDraw(animShader, currentFrame, modelCat);

        catPos2.y = terrain.getHeightWorld(catPos2.x, catPos2.z);
        glm::mat4 modelCat2 = glm::translate(glm::mat4(1.0f), catPos2);
        modelCat2 = glm::rotate(modelCat2, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        modelCat2 = glm::scale(modelCat2, glm::vec3(5.0f));
        animShader.use();
        cat2.externalState = IDLE;
        cat2.UpdateAndDraw(animShader, currentFrame, modelCat2);
        // ------------画赛车的模型---------------------
        ourShader.use();

        // 获取地形法线
        glm::vec3 normal = terrain.getNormalWorld(myCar.Position.x, myCar.Position.z);
        // 赛车水平朝向向量
        glm::vec3 forward = glm::vec3(sin(glm::radians(myCar.Heading)), 0, cos(glm::radians(myCar.Heading)));
        // 计算贴地的坐标系
        glm::vec3 right = glm::normalize(glm::cross(forward, normal));
        glm::vec3 actualForward = glm::normalize(glm::cross(normal, right));

        // 构建旋转矩阵
        glm::mat4 rotation = glm::mat4(1.0f);
        rotation[0] = glm::vec4(right, 0.0f);
        rotation[1] = glm::vec4(normal, 0.0f);
        rotation[2] = glm::vec4(actualForward, 0.0f);

        float carHeightOffset = 0.50f;
        glm::vec3 adjustedPose = myCar.Position;
        adjustedPose += carHeightOffset;

        glm::mat4 modelCar = glm::translate(glm::mat4(1.0f), adjustedPose);
        modelCar = modelCar * rotation; // 应用地形倾斜旋转
        modelCar = glm::scale(modelCar, glm::vec3(1.0f));

        ourShader.setMat4("model", modelCar);
        mclaren.DrawCar(ourShader, modelCar, myCar);

        // -------------画地形---------------------------
        terrain.render(view, projection, camera.Position);

        // ----------------- 画植被 -----------------
        vegetation->render(terrain, treeShader, view, projection, camera.Position);

        // ------------画天空盒（最后画天空盒！！不然其它模型会被它挡住）-----------------------
        skybox.draw(view, projection);
        // ------------画天空盒-----------------------------------------------------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;

}
