#include <iostream>
#include <filesystem> 
#include <GL/glew.h>      // 必须在 GLFW 之前包含（GLEW 要求）
#include <GLFW/glfw3.h>

// 引入 GLM 头文件（用于数学测试）
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

bool TestAssimpLoadModel(const char* modelPath) {
    // 检查文件是否存在
    if (!std::filesystem::exists(modelPath)) {
        std::cerr << "❌ Model file does not exist: " << modelPath << std::endl;
        return false;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(modelPath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->HasMeshes()) {
        std::cerr << "❌ Failed to load model: " << importer.GetErrorString() << std::endl;
        return false;
    }

    std::cout << "✅ Successfully loaded model: " << modelPath << std::endl;
    std::cout << "   Meshes: " << scene->mNumMeshes << std::endl;
    std::cout << "  ✅ Assimp worked! "<< "\n";

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        std::cout << "   - Mesh " << i << ": "
                  << mesh->mNumVertices << " vertices, "
                  << mesh->mNumFaces << " faces" << std::endl;
    }
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Testing GLEW + GLFW + OpenGL + GLM + Assimp..." << std::endl;
    std::cout << "========================================" << std::endl;
    // 先测试 Assimp（可放最前面）
    TestAssimpLoadModel("D:\\Github_documents\\CGFinal\\blender\\test1.obj");  // 确保该路径存在！
    
    // ----------------------------
    // 1. 测试 GLM（纯 CPU 计算）
    // ----------------------------
    {
        std::cout << "\n[1/3] Testing GLM...\n";
        glm::vec4 vec(1.0f, 0.0f, 0.0f, 1.0f);
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 0.0f));
        glm::vec4 result = trans * vec;

        std::cout << "  Original: (" << vec.x << ", " << vec.y << ", " << vec.z << ")\n";
        std::cout << "  Result:   (" << result.x << ", " << result.y << ", " << result.z << ")\n";

        if (result.x == 2.0f && result.y == 1.0f) {
            std::cout << "  ✅ GLM test PASSED.\n";
        }
        else {
            std::cout << "  ❌ GLM test FAILED.\n";
            return -1;
        }
    }

    // ----------------------------
    // 2. 初始化 GLFW
    // ----------------------------
    std::cout << "\n[2/3] Initializing GLFW...\n";
    if (!glfwInit()) {
        std::cerr << "  ❌ Failed to initialize GLFW!\n";
        return -1;
    }

    // 配置 GLFW：使用 OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(300, 200, "GLEW Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "  ❌ Failed to create GLFW window!\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ----------------------------
    // 3. 初始化 GLEW
    // ----------------------------
    std::cout << "\n[3/3] Initializing GLEW...\n";
    glewExperimental = GL_TRUE; // 必须设置（尤其在 Core Profile 下）
    if (glewInit() != GLEW_OK) {
        std::cerr << "  ❌ Failed to initialize GLEW!\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // 输出版本信息
    std::cout << "  OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "  GLEW Version:   " << glewGetString(GLEW_VERSION) << "\n";

    // ----------------------------
    // 4. 测试 OpenGL 函数指针（关键！）
    // ----------------------------
    std::cout << "\nTesting OpenGL function loading via GLEW...\n";
    GLuint vao = 0;
    glGenVertexArrays(1, &vao); // 这是 OpenGL 3.0+ 函数，必须通过 GLEW 加载

    if (vao != 0) {
        std::cout << "  ✅ glGenVertexArrays worked! VAO ID = " << vao << "\n";
        glDeleteVertexArrays(1, &vao);
    }
    else {
        std::cout << "  ❌ glGenVertexArrays failed!\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // ----------------------------
    // 清理
    // ----------------------------
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "\n========================================" << std::endl;
    std::cout << "🎉 ALL TESTS PASSED! GLEW is working!" << std::endl;
    std::cout << "You can now use modern OpenGL functions." << std::endl;
    std::cout << "========================================" << std::endl;

    // 防止控制台闪退（Windows）
#ifdef _WIN32
    std::cin.get();
#endif

    return 0;
}