#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "include/shader.hpp"

// ———————— 数据结构 ————————

struct AniVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

// 形态键数据
struct MorphTarget {
    std::vector<glm::vec3> Positions;
    std::vector<glm::vec3> Normals;
};

struct AniTexture {
    unsigned int id;
    std::string type;
    aiString path;
};

// ———————— Mesh 类 ————————

class AniMesh {
public:
    std::vector<AniVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<AniTexture> textures;
    std::vector<MorphTarget> morphTargets; // 存储该 Mesh 的所有形态键
    unsigned int VAO, VBO, EBO;
    glm::vec3 baseColor;

    AniMesh(std::vector<AniVertex> vertices, std::vector<unsigned int> indices, std::vector<AniTexture> textures, glm::vec3 color)
        : vertices(std::move(vertices)), indices(std::move(indices)), textures(std::move(textures)), baseColor(color) {
        setupMesh();
    }

    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(AniVertex), &vertices[0], GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AniVertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AniVertex), (void*)offsetof(AniVertex, Normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(AniVertex), (void*)offsetof(AniVertex, TexCoords));
        glBindVertexArray(0);
    }

    // 更新形态键插值
    void updateMorphAnimation(const std::vector<float>& weights) {
        if (morphTargets.empty() || weights.empty()) return;

        std::vector<AniVertex> animatedVertices = vertices;
        for (size_t i = 0; i < morphTargets.size() && i < weights.size(); ++i) {
            float weight = weights[i];
            if (weight <= 0.001f) continue;

            for (size_t j = 0; j < animatedVertices.size(); ++j) {
                animatedVertices[j].Position += morphTargets[i].Positions[j] * weight;
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, animatedVertices.size() * sizeof(AniVertex), &animatedVertices[0]);
    }

    void Draw(Shader& shader) {
        shader.setVec3("Material_baseColor", baseColor);
        bool hasDiffuseMap = false;
        for (unsigned int i = 0; i < textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            if (!hasDiffuseMap && textures[i].type == "texture_diffuse") {
                shader.setInt("texture_diffuse1", i);
                hasDiffuseMap = true;
            }
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }
        shader.setBool("hasTexture", hasDiffuseMap);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// ———————— Model 类 ————————

class AniModel {
public:
    std::vector<AniMesh> meshes;
    Assimp::Importer importer;
    const aiScene* scene = nullptr;
    std::string TEXTURES_DIR;
    int externalState = 0; // 状态机0 idle, 1 walk, 2 run

    AniModel(const std::string& path, const std::string& texture_path) : TEXTURES_DIR(texture_path) {
        loadModel(path);
    }

#include <cmath>
#include <vector>
#include <algorithm>

    std::vector<float> generateGaussianWaveWeights(float currentTimeMs, int numMorphs, float totalDurationMs, float sigma) {
        std::vector<float> weights(numMorphs, 0.0f);

        // 1. 循环时间处理
        float loopTime = fmod(currentTimeMs, totalDurationMs);
        if (loopTime < 0) loopTime += totalDurationMs;

        // 2. 计算中心索引 mu
        float mu = (loopTime / totalDurationMs) * static_cast<float>(numMorphs);

        float twoSigmaSq = 2.0f * sigma * sigma;

        // 3. 遍历计算
        for (int i = 0; i < numMorphs; ++i) {
            float diff = static_cast<float>(i) - mu;


            float w = 0.5f * std::exp(-(diff * diff) / twoSigmaSq);
            weights[i] = w;
        }

        return weights;
    }

    void UpdateAndDraw(Shader& shader, float currentTime, glm::mat4 baseTransform) {
        if (!scene || scene->mNumAnimations == 0) {
            // 如果没动画，直接画静态的
            std::cout << "no animation" << std::endl;
            drawNode(scene->mRootNode, shader, baseTransform);
            return;
        }

        const aiAnimation* anim = scene->mAnimations[0];

        // 确保 ticksPerSecond 有效
        float ticksPerSecond = (float)(anim->mTicksPerSecond != 0 ? anim->mTicksPerSecond : 25.0f);
        float timeInTicks = currentTime * ticksPerSecond;

        // 核心：fmod 确保时间在 0 到总长度之间循环
        float animationTime = fmod(timeInTicks, (float)anim->mDuration);

        std::cout << "Anim Time: " << animationTime << std::endl;

        // 强行测试
        float time = glfwGetTime();
        std::vector<float> testWeights(311, 0.0f);
        // 摇头
        //for (int i = 0; i < 5; i++) {
        //    testWeights[i] = (sin(time * 3.0f + i) + 1.0f) * 0.5f;
        //}

        // 使用正弦波模拟呼吸，周期约 3-4 秒
        //float breath = (sin(time * 1.5f) + 1.0f) * 0.5f; // 缩放到 0.0 - 1.0
        // 假设索引 10 是胸部扩张，11 是肩膀微抬
        //testWeights[10] = breath * 0.3f; // 呼吸幅度不宜过大
        //testWeights[11] = breath * 0.1f;

        // 高斯波权重
        // 完整动作
        //testWeights = generateGaussianWaveWeights(time * 1000.0f, 311, 13000.0f, 1.0f);
        // 好奇版摇头
        //std::vector<float> newWeights = generateGaussianWaveWeights(time * 1000.0f, 200, 8360.0f, 1.0f);
        //std::copy(newWeights.begin(), newWeights.begin() + 200, testWeights.begin());
        // 跑步
        //std::vector<float> newWeights = generateGaussianWaveWeights(time * 1000.0f, 32, 1338.0f, 1.0f);
        //std::copy(newWeights.begin(), newWeights.begin() + 32, testWeights.begin()+210);
        // 走路
        //std::vector<float> newWeights = generateGaussianWaveWeights(time * 1000.0f, 50, 2075.0f, 1.0f);
        //std::copy(newWeights.begin(), newWeights.begin() + 50, testWeights.begin() + 260);
        
        if (externalState == 2) {          // RUN
            auto w = generateGaussianWaveWeights(time * 1000.0f, 32, 1338.0f, 1.0f);
            std::copy(w.begin(), w.end(), testWeights.begin() + 210);
        }
        else if (externalState == 1) {     // WALK
            auto w = generateGaussianWaveWeights(time * 1000.0f, 50, 2075.0f, 1.0f);
            std::copy(w.begin(), w.end(), testWeights.begin() + 260);
        }
        else {                             // IDLE (摇头)
            auto w = generateGaussianWaveWeights(time * 1000.0f, 20, 4000.0f, 1.0f);
            std::copy(w.begin(), w.end(), testWeights.begin());
            //testWeights = generateGaussianWaveWeights(time * 1000.0f, 311, 13000.0f, 1.0f);
        }

        for (auto& m : meshes) {
            if (!m.morphTargets.empty()) {
                m.updateMorphAnimation(testWeights);
            }
        }

        drawAnimatedNode(scene->mRootNode, shader, baseTransform, animationTime);
    }

private:
    void loadModel(const std::string& path) {
        scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);
        if (scene && scene->mNumAnimations > 0) {
            for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
                aiAnimation* anim = scene->mAnimations[i];
                std::cout << "Animation [" << i << "]: " << anim->mName.C_Str()
                    << " | Duration: " << anim->mDuration
                    << " | Node Channels: " << anim->mNumChannels
                    << " | Mesh Channels: " << anim->mNumMeshChannels << std::endl;
            }
        }
        else {
            std::cout << "Error: No animations found in this file!" << std::endl;
        }

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "Assimp error: " << importer.GetErrorString() << std::endl;
            return;
        }
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    unsigned int TextureFromFile(const char* path, const std::string& directory) {
        std::string filename = std::string(path);
        filename = directory + "/" + filename; // 拼接完整路径

        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
        if (data) {
            GLenum format;
            if (nrComponents == 1) { format = GL_RED; } //std::cerr << "nrComponents == 1" << filename << std::endl; }
            else if (nrComponents == 3) { format = GL_RGB; }// std::cerr << "nrComponents == 3" << filename << std::endl; }
            else if (nrComponents == 4) { format = GL_RGBA; }//std::cerr << "nrComponents == 4" << filename << std::endl;}
            else {
                std::cerr << "Texture format not supported for " << filename << std::endl;
                stbi_image_free(data);
                return 0;
            }

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            // 设置纹理参数
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
        }
        else {
            std::cerr << "Texture failed to load at path: " << filename << std::endl;
            stbi_image_free(data);
            return 0;
        }

        return textureID;
    }

    AniMesh processMesh(aiMesh* mesh, const aiScene* scene) {
        std::vector<AniVertex> vertices;
        std::vector<unsigned int> indices;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            AniVertex v;
            v.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            if (mesh->mNormals) v.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            if (mesh->mTextureCoords[0]) v.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            vertices.push_back(v);
        }
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            for (unsigned int j = 0; j < mesh->mFaces[i].mNumIndices; j++) indices.push_back(mesh->mFaces[i].mIndices[j]);
        }

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        std::vector<AniTexture> textures;

        // 加载 Diffuse 贴图
        for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_DIFFUSE); i++) {
            aiString str;
            material->GetTexture(aiTextureType_DIFFUSE, i, &str);
            AniTexture texture;
            texture.id = TextureFromFile(str.C_Str(), this->TEXTURES_DIR);
            texture.type = "texture_diffuse";
            texture.path = str;
            textures.push_back(texture);
        }

        // 使用加载到的 textures 初始化 Mesh
        AniMesh newMesh(vertices, indices, textures, glm::vec3(1.0f));

        // --- 加载形态键 ---
        for (unsigned int i = 0; i < mesh->mNumAnimMeshes; i++) {
            aiAnimMesh* animMesh = mesh->mAnimMeshes[i];
            MorphTarget target;
            for (unsigned int j = 0; j < animMesh->mNumVertices; j++) {
                // Assimp 存储的是增量
                target.Positions.push_back(glm::vec3(animMesh->mVertices[j].x, animMesh->mVertices[j].y, animMesh->mVertices[j].z) - vertices[j].Position);
            }
            newMesh.morphTargets.push_back(target);
        }
        // 在 processMesh 函数末尾
        std::cout << "Mesh: " << mesh->mName.C_Str() << " has " << mesh->mNumAnimMeshes << " morph targets." << std::endl;
        return newMesh;
    }

    // --- 1. 基础递归渲染 ---
    void drawNode(aiNode* node, Shader& shader, glm::mat4 parentTransform) {
        glm::mat4 nodeTransform = convertMatrixToGLM(node->mTransformation);
        glm::mat4 globalTransform = parentTransform * nodeTransform;

        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            shader.setMat4("model", globalTransform);
            meshes[node->mMeshes[i]].Draw(shader);
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            drawNode(node->mChildren[i], shader, globalTransform);
        }
    }
    // --- 2. 动画递归渲染 ---
    void drawAnimatedNode(aiNode* node, Shader& shader, glm::mat4 parentTransform, float animationTime) {
        std::string nodeName(node->mName.data);
        glm::mat4 nodeTransform = convertMatrixToGLM(node->mTransformation);

        const aiAnimation* anim = scene->mAnimations[0];

        // 1. 处理节点位移动画
        const aiNodeAnim* nodeAnim = findNodeAnim(anim, nodeName);
        if (nodeAnim) {
            glm::vec3 translation = interpolatePosition(animationTime, nodeAnim);
            glm::quat rotation = interpolateRotation(animationTime, nodeAnim);
            glm::vec3 scale = interpolateScaling(animationTime, nodeAnim);
            nodeTransform = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
        }

        glm::mat4 globalTransform = parentTransform * nodeTransform;

        // 2. 处理该节点的 Meshes 和 Morph
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            int meshIdx = node->mMeshes[i];
            AniMesh& mesh = meshes[meshIdx];

            // 形态键权重可能存储在与“节点名”或“Mesh名”相同的通道中
            std::vector<float> allWeights;

            // 遍历所有 MeshChannels，寻找匹配当前节点的权重
            for (unsigned int j = 0; j < anim->mNumMeshChannels; j++) {
                if (std::string(anim->mMeshChannels[j]->mName.data) == nodeName) {
                    std::vector<float> w = interpolateWeights(animationTime, anim->mMeshChannels[j]);
                    allWeights.insert(allWeights.end(), w.begin(), w.end());
                }
            }

            if (!allWeights.empty()) {
                mesh.updateMorphAnimation(allWeights);
            }

            shader.setMat4("model", globalTransform);
            mesh.Draw(shader);
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            drawAnimatedNode(node->mChildren[i], shader, globalTransform, animationTime);
        }
    }

    // --- 辅助工具函数 ---
    const aiNodeAnim* findNodeAnim(const aiAnimation* anim, std::string name) {
        for (unsigned int i = 0; i < anim->mNumChannels; i++) {
            if (std::string(anim->mChannels[i]->mNodeName.data) == name) return anim->mChannels[i];
        }
        return nullptr;
    }

    const aiMeshAnim* findMeshAnim(const aiAnimation* anim, std::string name) {
        for (unsigned int i = 0; i < anim->mNumMeshChannels; i++) {
            if (std::string(anim->mMeshChannels[i]->mName.data) == name) return anim->mMeshChannels[i];
        }
        return nullptr;
    }

    std::vector<float> interpolateWeights(float time, const aiMeshAnim* anim) {
        std::vector<float> w;
        if (!anim || anim->mNumKeys == 0) return w;

        // 找到当前时间对应的关键帧
        unsigned int frame = 0;
        for (unsigned int i = 0; i < anim->mNumKeys - 1; i++) {
            if (time < (float)anim->mKeys[i + 1].mTime) {
                frame = i;
                break;
            }
        }

        // 计算插值因子
        unsigned int nextFrame = (frame + 1) % anim->mNumKeys;
        float t1 = (float)anim->mKeys[frame].mTime;
        float t2 = (float)anim->mKeys[nextFrame].mTime;
        float factor = 0.0f;
        if (t2 - t1 > 0.0001f) factor = (time - t1) / (t2 - t1);

        float weight = glm::mix((float)anim->mKeys[frame].mValue, (float)anim->mKeys[nextFrame].mValue, factor);
        w.push_back(weight);

        return w;
    }

    // 矩阵转换
    glm::mat4 convertMatrixToGLM(const aiMatrix4x4& from) {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    // 插值辅助 (Position, Rotation, Scale)
    glm::vec3 interpolatePosition(float time, const aiNodeAnim* anim) {
        if (anim->mNumPositionKeys == 1) return glm::vec3(anim->mPositionKeys[0].mValue.x, anim->mPositionKeys[0].mValue.y, anim->mPositionKeys[0].mValue.z);
        unsigned int i = 0;
        for (; i < anim->mNumPositionKeys - 1; i++) if (time < (float)anim->mPositionKeys[i + 1].mTime) break;
        float factor = (time - (float)anim->mPositionKeys[i].mTime) / ((float)anim->mPositionKeys[i + 1].mTime - (float)anim->mPositionKeys[i].mTime);
        aiVector3D start = anim->mPositionKeys[i].mValue, end = anim->mPositionKeys[i + 1].mValue;
        return glm::mix(glm::vec3(start.x, start.y, start.z), glm::vec3(end.x, end.y, end.z), factor);
    }

    glm::quat interpolateRotation(float time, const aiNodeAnim* anim) {
        if (anim->mNumRotationKeys == 1) return glm::quat(anim->mRotationKeys[0].mValue.w, anim->mRotationKeys[0].mValue.x, anim->mRotationKeys[0].mValue.y, anim->mRotationKeys[0].mValue.z);
        unsigned int i = 0;
        for (; i < anim->mNumRotationKeys - 1; i++) if (time < (float)anim->mRotationKeys[i + 1].mTime) break;
        float factor = (time - (float)anim->mRotationKeys[i].mTime) / ((float)anim->mRotationKeys[i + 1].mTime - (float)anim->mRotationKeys[i].mTime);
        aiQuaternion start = anim->mRotationKeys[i].mValue, end = anim->mRotationKeys[i + 1].mValue;
        return glm::slerp(glm::quat(start.w, start.x, start.y, start.z), glm::quat(end.w, end.x, end.y, end.z), factor);
    }

    glm::vec3 interpolateScaling(float time, const aiNodeAnim* anim) {
        if (anim->mNumScalingKeys == 1) return glm::vec3(anim->mScalingKeys[0].mValue.x, anim->mScalingKeys[0].mValue.y, anim->mScalingKeys[0].mValue.z);
        unsigned int i = 0;
        for (; i < anim->mNumScalingKeys - 1; i++) if (time < (float)anim->mScalingKeys[i + 1].mTime) break;
        float factor = (time - (float)anim->mScalingKeys[i].mTime) / ((float)anim->mScalingKeys[i + 1].mTime - (float)anim->mScalingKeys[i].mTime);
        aiVector3D start = anim->mScalingKeys[i].mValue, end = anim->mScalingKeys[i + 1].mValue;
        return glm::mix(glm::vec3(start.x, start.y, start.z), glm::vec3(end.x, end.y, end.z), factor);
    }
};