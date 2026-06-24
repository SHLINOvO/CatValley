#pragma once
#include <memory>
#include <string>

#include "include/vegetation/VegetationManager.hpp"
#include "include/terrain/terrain.hpp"

// ============================================================
// CreateDefaultVegetationManager (适配新版 VegetationManager)
// - 不使用高度/坡度约束：只做随机种植（贴地）
// - 密度与间距策略：果树略密、阔叶中等、针叶稀疏、灌木更密
// ============================================================
inline std::unique_ptr<VegetationManager> CreateDefaultVegetationManager(
    Terrain& terrain,
    int seed,
    float worldMinX, float worldMaxX,
    float worldMinZ, float worldMaxZ,
    glm::vec2 flowerBedCenter,
    float flowerBedRadius
) {
    using Type = VegetationManager::SpeciesType;

    auto veg = std::make_unique<VegetationManager>();

    const std::string treeDir = std::string(ASSETS_FOLDER) + "vegetation/trees/";

    // =========================================================
    // 果树 / 庭院树（略密）
    // - density 稍高
    // - spacing 稍小（但仍要保持果园观感）
    // =========================================================
    veg->addSpecies(VegetationManager::Species(
        "AppleTree",
        Type::OrchardFruit,
        treeDir + "apple_tree.glb",
        treeDir,
        26.0f,          // targetHeight
        0.90f, 1.10f,   // scale jitter
        0.000080f,      // density（略密）
        14.0f           // minSpacing
    ));

    veg->addSpecies(VegetationManager::Species(
        "CherryTree",
        Type::OrchardFruit,
        treeDir + "cherry_tree.glb",
        treeDir,
        24.0f,
        0.88f, 1.05f,
        0.000082f,
        14.0f
    ));

    veg->addSpecies(VegetationManager::Species(
        "PlumTree",
        Type::OrchardFruit,
        treeDir + "plum_tree.glb",
        treeDir,
        25.0f,
        0.90f, 1.08f,
        0.000080f,
        14.0f
    ));

    // =========================================================
    // 阔叶林（中等）
    // - density 中等
    // - spacing 中等
    // =========================================================
    veg->addSpecies(VegetationManager::Species(
        "Birch1",
        Type::DeciduousTree,
        treeDir + "birch1_20.glb",
        treeDir,
        30.0f,
        0.85f, 1.15f,
        0.000048f,   // 中等
        16.0f
    ));

    veg->addSpecies(VegetationManager::Species(
        "Birch2",
        Type::DeciduousTree,
        treeDir + "birch2_21.glb",
        treeDir,
        29.0f,
        0.85f, 1.12f,
        0.000046f,
        16.0f
    ));

    veg->addSpecies(VegetationManager::Species(
        "Birch3",
        Type::DeciduousTree,
        treeDir + "birch3_26.glb",
        treeDir,
        32.0f,
        0.88f, 1.15f,
        0.000042f,
        17.0f
    ));

    veg->addSpecies(VegetationManager::Species(
        "Maple",
        Type::DeciduousTree,
        treeDir + "maple.glb",
        treeDir,
        36.0f,
        0.90f, 1.18f,
        0.000028f,
        18.0f
    ));

    veg->addSpecies(VegetationManager::Species(
        "GreenTree",
        Type::DeciduousTree,
        treeDir + "green_tree.glb",
        treeDir,
        34.0f,
        0.88f, 1.15f,
        0.000040f,
        17.0f
    ));

     // 加回 green_tree2 / green_tree_3（阔叶中等策略内，略低 density 避免“刷墙”）
    veg->addSpecies(VegetationManager::Species(
        "GreenTree2",
        Type::DeciduousTree,
        treeDir + "green_tree_2.glb",
        treeDir,
        35.0f,
        0.90f, 1.20f,
        0.000028f,
        17.0f
    ));

    veg->addSpecies(VegetationManager::Species(
        "GreenTree3",
        Type::DeciduousTree,
        treeDir + "green_tree_3.glb",
        treeDir,
        33.0f,
        0.90f, 1.15f,
        0.000028f,
        17.0f
    ));

    // =========================================================
    // 针叶林（稀疏）
    // - density 更低
    // - spacing 更大（突出“点缀/疏林”效果）
    // =========================================================
    veg->addSpecies(VegetationManager::Species(
        "FirTree",
        Type::ConiferTree,
        treeDir + "fir_tree.glb",
        treeDir,
        42.0f,
        0.90f, 1.20f,
        0.000042f,   // 稀疏
        22.0f
    ));

    // =========================================================
    // 灌木 / 林下层（更密）
    // - density 更高
    // - spacing 更小
    // =========================================================
    veg->addSpecies(VegetationManager::Species(
        "HollyShrub",
        Type::Shrub,
        treeDir + "holly_shrub.glb",
        treeDir,
        6.0f,
        0.90f, 1.25f,
        0.000190f,   // 更密
        6.0f
    ));

    //veg->addSpecies(VegetationManager::Species(
    //    "DeciduousShrub",
    //    Type::Shrub,
    //    treeDir + "deciduousShrub2.glb",
    //    treeDir,
    //    5.0f,
    //    0.90f, 1.25f,
    //    0.000220f,
    //    6.0f
    //));

    // =========================================================
// 花 / 草（GroundCover）——用于“圆形花坪”
// =========================================================
    const std::string flowerDir = std::string(ASSETS_FOLDER) + "vegetation/flowers/";
    std::vector<int> bedSpecies;
    bedSpecies.reserve(6);

    // 目标高度与间距需要根据模型实际尺度微调：以下是一套“先能很密”的默认值
    bedSpecies.push_back(veg->addSpecies(VegetationManager::Species(
        "ColoredFlower",
        Type::GroundCover,
        flowerDir + "colored_flower.glb",
        flowerDir,
        0.55f,          // targetHeight
        0.85f, 1.15f,
        1.0f,           // density 对花坪生成函数不强依赖，可随意
        0.55f           // minSpacing
    )));

    bedSpecies.push_back(veg->addSpecies(VegetationManager::Species(
        "Daisy",
        Type::GroundCover,
        flowerDir + "daisy.glb",
        flowerDir,
        0.50f,
        0.85f, 1.15f,
        1.0f,
        0.52f
    )));

    bedSpecies.push_back(veg->addSpecies(VegetationManager::Species(
        "Flower",
        Type::GroundCover,
        flowerDir + "flower.glb",
        flowerDir,
        0.60f,
        0.85f, 1.18f,
        1.0f,
        0.58f
    )));

    bedSpecies.push_back(veg->addSpecies(VegetationManager::Species(
        "GrassYellowFlower",
        Type::GroundCover,
        flowerDir + "grass_with_yellow_flower.glb",
        flowerDir,
        0.45f,
        0.85f, 1.12f,
        1.0f,
        0.45f
    )));

    bedSpecies.push_back(veg->addSpecies(VegetationManager::Species(
        "GreenGrass",
        Type::GroundCover,
        flowerDir + "green_grass.glb",
        flowerDir,
        0.40f,
        0.85f, 1.12f,
        1.0f,
        0.32f
    )));

    bedSpecies.push_back(veg->addSpecies(VegetationManager::Species(
        "RoseFlower",
        Type::GroundCover,
        flowerDir + "rose_flower.glb",
        flowerDir,
        0.65f,
        0.85f, 1.20f,
        1.0f,
        0.62f
    )));


    // =========================================================
    // 初始化 & 生成
    // =========================================================
    veg->initModels();
    veg->generate(terrain, seed, worldMinX, worldMaxX, worldMinZ, worldMaxZ);

    // --- 生成花坪（圆形）---
    veg->generateFlowerBedCircle(
        terrain,
        seed + 999,      // 独立 seed
        flowerBedCenter,
        flowerBedRadius,
        bedSpecies,
        0.7f,           // 覆盖率：越大越“看不到地面”
        90               // 重试次数：越大越密但生成更慢
    );


    return veg;
}
