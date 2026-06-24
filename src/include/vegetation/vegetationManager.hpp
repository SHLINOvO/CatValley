#pragma once
#include <string>
#include <vector>
#include <random>
#include <unordered_map>
#include <memory>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../shader.hpp"
#include "../model.hpp"
#include "../terrain/terrain.hpp"

class VegetationManager {
public:
    enum class SpeciesType {
        OrchardFruit,
        DeciduousTree,
        ConiferTree,
        Shrub,
        GroundCover   
    };


    struct Species {
        std::string name;
        SpeciesType type = SpeciesType::DeciduousTree;
        std::string modelPath;
        std::string textureDir;

        // 统一尺度：把模型高度归一到 targetHeight
        float targetHeight = 35.0f;

        // 随机缩放扰动
        float minScaleJitter = 0.85f;
        float maxScaleJitter = 1.20f;

        // 分布参数
        float density = 0.00008f;  // “每平方单位”的概率因子（近似）
        float minSpacing = 12.0f;  // 最小间距（同类）

        Species() = default;

        // 显式构造：便于在 vegetation.hpp 里直接 VegetationManager::Species(...)
        Species(
            std::string inName,
            SpeciesType inType,
            std::string inModelPath,
            std::string inTextureDir,

            float inTargetHeight = 35.0f,
            float inMinScaleJitter = 0.85f,
            float inMaxScaleJitter = 1.20f,

            float inDensity = 0.00008f,
            float inMinSpacing = 12.0f
        )
            : name(std::move(inName))
            , type(inType)
            , modelPath(std::move(inModelPath))
            , textureDir(std::move(inTextureDir))
            , targetHeight(inTargetHeight)
            , minScaleJitter(inMinScaleJitter)
            , maxScaleJitter(inMaxScaleJitter)
            , density(inDensity)
            , minSpacing(inMinSpacing)
        {
        }
    };

    struct Instance {
        int speciesIndex = -1;
        glm::vec3 pos{ 0.0f };
        float yawRad = 0.0f;
        float uniformScale = 1.0f;
    };

public:
    int addSpecies(const Species& sp) {
        species_.push_back(sp);
        return (int)species_.size() - 1;
    }



    void initModels() {
        models_.clear();
        models_.reserve(species_.size());
        for (const auto& sp : species_) {
            models_.push_back(std::make_unique<Model>(sp.modelPath, sp.textureDir));
        }
    }

    // 纯随机布置：只贴地 + 密度 + 最小间距
    void generate(const Terrain& terrain,
        int seed,
        float worldMinX, float worldMaxX,
        float worldMinZ, float worldMaxZ)
    {
        instances_.clear();
        spacingGrid_.clear();
        rng_.seed(seed);

        // 统一采样网格步长：取所有物种 minSpacing 的一个合理基准
        float baseStep = 8.0f;
        for (const auto& sp : species_) {
            baseStep = std::min(baseStep, sp.minSpacing * 0.5f);
        }
        baseStep = glm::clamp(baseStep, 2.0f, 14.0f);

        for (float z = worldMinZ; z <= worldMaxZ; z += baseStep) {
            for (float x = worldMinX; x <= worldMaxX; x += baseStep) {

                for (int si = 0; si < (int)species_.size(); ++si) {
                    const Species& sp = species_[si];

                    if (sp.type == SpeciesType::GroundCover) {
                        continue;
                    }

                    // 把 density 转成每个采样点的接受概率（近似：density * cellArea）
                    float p = glm::clamp(sp.density * baseStep * baseStep, 0.0f, 0.4f);
                    if (rand01_() > p) continue;

                    // 抖动，避免网格感
                    float px = x + (rand01_() - 0.5f) * baseStep;
                    float pz = z + (rand01_() - 0.5f) * baseStep;

                    // 贴地
                    float py = terrain.getHeightWorld(px, pz);

                    // 最小间距过滤（同类）
                    if (!checkSpacing_(sp, { px, py, pz })) continue;

                    Instance inst;
                    inst.speciesIndex = si;
                    inst.pos = { px, py, pz };
                    inst.yawRad = rand01_() * glm::two_pi<float>();
                    inst.uniformScale = randRange_(sp.minScaleJitter, sp.maxScaleJitter);

                    instances_.push_back(inst);
                    addToSpacingGrid_(sp, inst.pos);
                }
            }
        }
    }

    // 注意：Model::Draw 非 const，所以 render 也非 const（不改 Model 的前提下）
    void render(const Terrain&,
        Shader& shader,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& cameraPos)
    {
        shader.use();
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        for (const auto& inst : instances_) {
            const Species& sp = species_[inst.speciesIndex];
            Model& model = *models_[inst.speciesIndex];

            // 简单距离裁剪
            float maxDist = 800.0f;
            switch (sp.type) {
            case SpeciesType::GroundCover: maxDist = 140.0f; break;
            case SpeciesType::Shrub:       maxDist = 250.0f; break;
            default:                       maxDist = 800.0f; break;
            }
            glm::vec3 d = inst.pos - cameraPos;
            if (glm::dot(d, d) > maxDist * maxDist) continue;

            // 归一到 targetHeight
            float h = model.getAabbHeight();
            float scale = (h > 1e-6f) ? (sp.targetHeight / h) : 1.0f;
            scale *= inst.uniformScale;

            // 1. 模型空间：归一化 + 缩放
            glm::mat4 local(1.0f);
            local = local * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
            local = local * model.getNormalizeTransform(true, true);

            // 2. 世界空间：旋转 + 平移
            glm::mat4 M(1.0f);
            M = glm::translate(M, inst.pos);
            M = glm::rotate(M, inst.yawRad, { 0, 1, 0 });

            // 3. 合成
            M = M * local;

            model.Draw(shader, M);

        }
    }

    // 在 VegetationManager public: 里添加
    void generateFlowerBedCircle(
        const Terrain& terrain,
        int seed,
        const glm::vec2& centerXZ,
        float radius,
        const std::vector<int>& speciesIndices,   // 参与混合的 GroundCover 物种下标
        float targetCoverage = 0.92f,             // 目标覆盖率（越大越密）
        int maxTriesPerPoint = 20                 // 失败重试上限，控制性能
    ) {
        if (speciesIndices.empty() || radius <= 0.0f) return;

        // 使用独立 seed，避免影响主 generate() 的随机序列（也方便复现）
        rng_.seed(seed);

        // 找参与物种里最小 minSpacing，决定“理论最大密度”
        float smin = FLT_MAX;
        for (int idx : speciesIndices) {
            if (idx < 0 || idx >= (int)species_.size()) continue;
            smin = std::min(smin, std::max(species_[idx].minSpacing, 0.05f));
        }
        if (smin == FLT_MAX) return;

        // 估算目标数量：圆面积 / (spacing^2) * coverage系数
        // coverage 越接近 1，越接近“几乎看不到地面”
        const float area = glm::pi<float>() * radius * radius;
        int targetCount = (int)std::ceil(area / (smin * smin) * targetCoverage);


        // 为了避免 spacingGrid_ 被之前树的 cell size 干扰，这里不复用 spacingGrid_
        // 采用一个局部 grid（只对花坪实例做间距约束）
        struct LocalCellKey { int x, z; bool operator==(const LocalCellKey& o) const { return x == o.x && z == o.z; } };
        struct LocalHash {
            size_t operator()(const LocalCellKey& k) const noexcept {
                return (std::hash<int>()(k.x) * 73856093u) ^ (std::hash<int>()(k.z) * 19349663u);
            }
        };
        std::unordered_map<LocalCellKey, std::vector<glm::vec3>, LocalHash> localGrid;

        auto cellOfLocal = [&](float cellSize, const glm::vec3& p) -> LocalCellKey {
            return { (int)std::floor(p.x / cellSize), (int)std::floor(p.z / cellSize) };
            };
        auto checkLocal = [&](float cellSize, float minSpacing, const glm::vec3& p) -> bool {
            const float r2 = minSpacing * minSpacing;
            LocalCellKey c = cellOfLocal(cellSize, p);
            for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) {
                auto it = localGrid.find({ c.x + dx, c.z + dz });
                if (it == localGrid.end()) continue;
                for (const auto& q : it->second) {
                    glm::vec2 d(p.x - q.x, p.z - q.z);
                    if (glm::dot(d, d) < r2) return false;
                }
            }
            return true;
            };
        auto addLocal = [&](float cellSize, const glm::vec3& p) {
            localGrid[cellOfLocal(cellSize, p)].push_back(p);
            };

        // cellSize 取 smin（Poisson grid 常用策略）
        const float cellSize = smin;

        int placed = 0;
        int globalTries = 0;
        int globalTryLimit = targetCount * maxTriesPerPoint;

        while (placed < targetCount && globalTries < globalTryLimit) {
            ++globalTries;

            // 圆内均匀采样：r = sqrt(u)*R
            float u = rand01_();
            float v = rand01_();
            float r = std::sqrt(u) * radius;
            float theta = v * glm::two_pi<float>();

            float px = centerXZ.x + r * std::cos(theta);
            float pz = centerXZ.y + r * std::sin(theta);
            float py = terrain.getHeightWorld(px, pz);

            // 随机选择一种参与物种（均匀混合；你也可以改成带权重）
            int pick = speciesIndices[(int)std::floor(rand01_() * speciesIndices.size()) % speciesIndices.size()];
            if (pick < 0 || pick >= (int)species_.size()) continue;
            const Species& sp = species_[pick];

            // 做过滤（更真实：草更密、花略稀）
            float spacing = std::max(sp.minSpacing, 0.01f);

            // 草：允许更密（甚至不限制）
            if (sp.name.find("Grass") != std::string::npos) {
                spacing *= 0.85f;           // 让草更密（等效 spacing 大幅降低）
            }

            if (spacing > 1e-6f) {
                if (!checkLocal(cellSize, spacing, { px,py,pz })) continue;
            }

            Instance inst;
            inst.speciesIndex = pick;
            inst.pos = { px, py, pz };
            inst.yawRad = rand01_() * glm::two_pi<float>();
            inst.uniformScale = randRange_(sp.minScaleJitter, sp.maxScaleJitter);

            instances_.push_back(inst);
            addLocal(cellSize, inst.pos);
            ++placed;
        }
    }


    size_t instanceCount() const { return instances_.size(); }

private:
    // ---- spacing grid（同类最小间距）----
    struct CellKey {
        int x, z;
        bool operator==(const CellKey& o) const { return x == o.x && z == o.z; }
    };
    struct CellKeyHash {
        size_t operator()(const CellKey& k) const noexcept {
            return (std::hash<int>()(k.x) * 73856093u)
                ^ (std::hash<int>()(k.z) * 19349663u);
        }
    };

    CellKey cellOf_(const Species& sp, const glm::vec3& p) const {
        float cs = std::max(sp.minSpacing, 1.0f);
        return { int(std::floor(p.x / cs)), int(std::floor(p.z / cs)) };
    }

    bool checkSpacing_(const Species& sp, const glm::vec3& p) const {
        float r2 = sp.minSpacing * sp.minSpacing;
        CellKey c = cellOf_(sp, p);

        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                auto it = spacingGrid_.find({ c.x + dx, c.z + dz });
                if (it == spacingGrid_.end()) continue;
                for (const auto& q : it->second) {
                    glm::vec2 d(p.x - q.x, p.z - q.z);
                    if (glm::dot(d, d) < r2) return false;
                }
            }
        }
        return true;
    }

    void addToSpacingGrid_(const Species& sp, const glm::vec3& p) {
        spacingGrid_[cellOf_(sp, p)].push_back(p);
    }

    // ---- random helpers ----
    float rand01_() const {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_);
    }
    float randRange_(float a, float b) const {
        return std::uniform_real_distribution<float>(a, b)(rng_);
    }

private:
    std::vector<Species> species_;
    std::vector<std::unique_ptr<Model>> models_;
    std::vector<Instance> instances_;

    std::unordered_map<CellKey, std::vector<glm::vec3>, CellKeyHash> spacingGrid_;
    mutable std::mt19937 rng_{ 1337 };
};
