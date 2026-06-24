#pragma once

#include "include/shader.hpp"
#include "include/car.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <cfloat>

// ---------------- Mesh ----------------
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct Texture {
    unsigned int id = 0;
    std::string type;
    aiString path;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    glm::vec3 baseColor = glm::vec3(1.0f);

    Mesh(std::vector<Vertex> v, std::vector<unsigned int> i, std::vector<Texture> t, glm::vec3 color = glm::vec3(1.0f))
        : vertices(std::move(v)), indices(std::move(i)), textures(std::move(t)), baseColor(color)
    {
        setupMesh();
    }

    void Draw(Shader& shader) {
        shader.setVec3("Material_baseColor", baseColor);

        bool hasDiffuseMap = false;

        // 仅绑定第一张 diffuse 到 texture_diffuse1
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
        glActiveTexture(GL_TEXTURE0);
    }

private:
    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        glBindVertexArray(0);
    }
};

// ---------------- Texture helpers ----------------
inline unsigned int TextureFromFile(const char* path, const std::string& directory) {
    std::string filename = directory + "/" + std::string(path);

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);

    int width = 0, height = 0, nrComponents = 0;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (!data) {
        std::cerr << "Texture failed to load: " << filename << std::endl;
        return 0;
    }

    GLenum format = GL_RGBA;
    if (nrComponents == 1) format = GL_RED;
    else if (nrComponents == 3) format = GL_RGB;
    else if (nrComponents == 4) format = GL_RGBA;
    else {
        std::cerr << "Unsupported texture components: " << nrComponents << " for " << filename << std::endl;
        stbi_image_free(data);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

inline unsigned int TextureFromMemory(const aiTexture* aiTex) {
    if (!aiTex) return 0;

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);

    int width = 0, height = 0, nrComponents = 0;
    unsigned char* data = nullptr;

    if (aiTex->mHeight == 0) {
        // 压缩图片（png/jpg）
        stbi_set_flip_vertically_on_load(false);
        data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(aiTex->pcData), aiTex->mWidth,
            &width, &height, &nrComponents, 0);
    }
    else {
        width = aiTex->mWidth;
        height = aiTex->mHeight;
        data = reinterpret_cast<unsigned char*>(aiTex->pcData);
        nrComponents = 4;
    }

    if (!data) return 0;

    GLenum format = GL_RGBA;
    if (nrComponents == 1) format = GL_RED;
    else if (nrComponents == 3) format = GL_RGB;
    else if (nrComponents == 4) format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (aiTex->mHeight == 0) stbi_image_free(data);
    return textureID;
}

// ---------------- Model ----------------
class Model {
public:
    std::vector<Texture> textures_loaded;
    std::vector<Mesh> meshes;

    std::string directory;
    std::string TEXTURES_DIR;

    Assimp::Importer importer;
    const aiScene* scene = nullptr;

    // AABB in "scene final space" (after node transforms)
    glm::vec3 aabbMin = glm::vec3(FLT_MAX);
    glm::vec3 aabbMax = glm::vec3(-FLT_MAX);
    bool aabbValid = false;

public:
    Model(const std::string& path, const std::string& texture_path)
        : TEXTURES_DIR(texture_path)
    {
        loadModel(path);
    }

    void Draw(Shader& shader, glm::mat4 baseTransform = glm::mat4(1.0f)) {
        if (scene && scene->mRootNode) {
            drawNode(scene->mRootNode, shader, baseTransform);
        }
    }

    void DrawCar(Shader& shader, const glm::mat4& baseTransform, Car& car) {
        if (scene && scene->mRootNode) {
            drawNodeCar(scene->mRootNode, shader, baseTransform, car);
        }
    }

    glm::vec3 getAabbCenter() const { return (aabbMin + aabbMax) * 0.5f; }
    glm::vec3 getAabbSize()   const { return (aabbMax - aabbMin); }
    float getAabbHeight()     const { return (aabbMax.y - aabbMin.y); }

    // 归一化：基于【最终AABB】而不是 mesh 局部AABB
    glm::mat4 getNormalizeTransform(bool centerXZ = true, bool liftToGround = true) const {
        glm::mat4 T(1.0f);
        if (!aabbValid) return T;

        glm::vec3 center = getAabbCenter();
        glm::vec3 offset(0.0f);

        if (centerXZ) {
            offset.x = -center.x;
            offset.z = -center.z;
        }
        if (liftToGround) {
            offset.y = -aabbMin.y;
        }
        else {
            offset.y = -center.y;
        }

        return glm::translate(glm::mat4(1.0f), offset);
    }

private:
    void loadModel(const std::string& path) {
        unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_FlipUVs; // 你原来就开着，先不动

        scene = importer.ReadFile(path, flags);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
            std::cout << "Assimp error: " << importer.GetErrorString() << std::endl;
            return;
        }

        std::cout << "[Model] Loaded OK. Meshes: " << scene->mNumMeshes
            << ", Materials: " << scene->mNumMaterials
            << ", Root: " << scene->mRootNode->mName.C_Str()
            << std::endl;

        directory = path.substr(0, path.find_last_of('/'));

        meshes.clear();
        textures_loaded.clear();

        processNode(scene->mRootNode, scene);

        // 关键：处理完 meshes 之后，用节点树的最终变换来计算 AABB
        computeSceneAABB();
    }

    // ---------- Correct AABB computation (includes node transforms) ----------
    void computeSceneAABB() {
        aabbMin = glm::vec3(FLT_MAX);
        aabbMax = glm::vec3(-FLT_MAX);
        aabbValid = false;

        if (!scene || !scene->mRootNode) return;

        glm::mat4 I(1.0f);
        computeNodeAABB(scene->mRootNode, I);

        if (!aabbValid) {
            std::cerr << "[Model] Warning: AABB invalid (no vertices?)" << std::endl;
        }
        else {
            // 可选：输出 AABB 方便你调试
            // std::cout << "[Model] AABB min=(" << aabbMin.x << "," << aabbMin.y << "," << aabbMin.z << ") "
            //           << "max=(" << aabbMax.x << "," << aabbMax.y << "," << aabbMax.z << ")\n";
        }
    }

    void computeNodeAABB(aiNode* node, const glm::mat4& parent) {
        glm::mat4 nodeT = convertMatrixToGLM(node->mTransformation);
        glm::mat4 global = parent * nodeT;

        // node 引用的 mesh，顶点要乘 global 后计入 AABB
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            unsigned int meshIndex = node->mMeshes[i];
            if (meshIndex >= scene->mNumMeshes) continue;

            aiMesh* m = scene->mMeshes[meshIndex];
            if (!m || !m->mVertices) continue;

            for (unsigned int v = 0; v < m->mNumVertices; ++v) {
                glm::vec4 p(m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z, 1.0f);
                glm::vec3 pw = glm::vec3(global * p);

                aabbValid = true;
                aabbMin = glm::min(aabbMin, pw);
                aabbMax = glm::max(aabbMax, pw);
            }
        }

        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
            computeNodeAABB(node->mChildren[c], global);
        }
    }

    // ---------- rendering ----------
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

    void drawNodeCar(aiNode* node, Shader& shader, glm::mat4 parentTransform, Car& car) {
        glm::mat4 nodeTransform = convertMatrixToGLM(node->mTransformation);
        std::string name = node->mName.C_Str();

        if (name.find("front_tire") != std::string::npos || name.find("rear_tire") != std::string::npos) {
            glm::mat4 originNodeTransform = convertMatrixToGLM(node->mTransformation);

            glm::mat4 localRot(1.0f);
            if (name.find("front_tire") != std::string::npos) {
                localRot = glm::rotate(localRot, glm::radians(-car.SteerAngle), glm::vec3(0, 1, 0));
            }
            nodeTransform = originNodeTransform * localRot;
        }

        glm::mat4 globalTransform = parentTransform * nodeTransform;

        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            shader.setMat4("model", globalTransform);
            meshes[node->mMeshes[i]].Draw(shader);
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            drawNodeCar(node->mChildren[i], shader, globalTransform, car);
        }
    }

    // ---------- Assimp -> GLM ----------
    glm::mat4 convertMatrixToGLM(const aiMatrix4x4& from) {
        glm::mat4 to;
        to[0][0] = from.a1; to[0][1] = from.b1; to[0][2] = from.c1; to[0][3] = from.d1;
        to[1][0] = from.a2; to[1][1] = from.b2; to[1][2] = from.c2; to[1][3] = from.d2;
        to[2][0] = from.a3; to[2][1] = from.b3; to[2][2] = from.c3; to[2][3] = from.d3;
        to[3][0] = from.a4; to[3][1] = from.b4; to[3][2] = from.c4; to[3][3] = from.d4;
        return to;
    }

    // ---------- mesh extraction ----------
    void processNode(aiNode* node, const aiScene* sc) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = sc->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, sc));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], sc);
        }
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* sc) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        std::cout << "[Mesh] name=" << mesh->mName.C_Str()
            << " verts=" << mesh->mNumVertices
            << " faces=" << mesh->mNumFaces
            << std::endl;

        vertices.reserve(mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            if (mesh->mNormals) {
                vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }
            else {
                vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (mesh->mTextureCoords[0]) {
                vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }
            else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        aiMaterial* material = sc->mMaterials[mesh->mMaterialIndex];

        // diffuse / baseColor map
        auto diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", sc);
        if (diffuseMaps.empty()) {
            diffuseMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse", sc);
        }
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        aiColor4D color(1, 1, 1, 1);
        if (AI_SUCCESS != aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color)) {
            aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color);
        }

        return Mesh(std::move(vertices), std::move(indices), std::move(textures), glm::vec3(color.r, color.g, color.b));
    }

    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName, const aiScene* sc) {
        std::vector<Texture> textures;

        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);

            bool skip = false;
            for (const auto& loadedTex : textures_loaded) {
                if (loadedTex.path == str) {
                    textures.push_back(loadedTex);
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            Texture texture;
            texture.type = typeName;
            texture.path = str;
            texture.id = 0;

            std::string key = str.C_Str();
            if (!key.empty() && key[0] == '*') {
                const aiTexture* aiTex = sc->GetEmbeddedTexture(key.c_str());
                if (aiTex) texture.id = TextureFromMemory(aiTex);
            }
            else if (!key.empty()) {
                texture.id = TextureFromFile(key.c_str(), TEXTURES_DIR);
            }

            if (texture.id != 0) {
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
            else {
                std::cerr << "Failed to load texture: " << key << " (dir=" << TEXTURES_DIR << ")\n";
            }
        }

        return textures;
    }
};
