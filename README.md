# Cat Valley

基于 OpenGL 3.3+ 的 3D 户外场景渲染项目。在一个程序化生成的地形上，驾驶赛车穿梭于猫猫花园之间——草地、树林、花丛、湖泊，配合实时阴影、水面反射与 PBR 光照。

## 功能特性

### 地形系统
- **高度图地形**：2049×2049 高度图驱动的程序化地形，gridScale 可配置
- **多级 LOD（Level of Detail）**：根据摄像机距离动态切换 4 级细节层级（33→17→9→1 顶点间隔），远处地形自动降面
- **视锥体裁剪**：不可见的地形 Chunk 被剔除，减少绘制开销
- **法线计算**：基于高度图梯度实时计算地形法线，支持车辆贴地行驶
- **程序化湖泊**：在高度图上动态挖出圆形湖泊凹陷，带边缘渐变过渡

### 水面渲染
- **平面反射（Planar Reflection）**：通过 FBO 将场景以镜像摄像机渲染到反射纹理
- **Clip Plane 裁剪**：反射 Pass 裁剪水面以下内容，避免穿模
- **屏幕空间 UV 采样**：使用 `gl_Position` 屏幕投影坐标对齐反射纹理
- **波浪动画**：基于时间的正弦波顶点位移 + UV 扰动
- **半透明混合**：Alpha 混合 + 关闭深度写入，水面不遮挡后方透明物体

### 阴影系统
- **Shadow Mapping**：2048×2048 阴影贴图，正交投影覆盖场景
- **光源跟随**：阴影正交中心每帧跟随摄像机/赛车，保证视野内阴影精度
- **Shadow Acne 消除**：三管齐下——`glPolygonOffset(2.0, 4.0)` + 合理 bias + 缩小正交范围
- **多物体阴影**：地形、车辆、猫、植被均投射阴影

### PBR 光照
- 基于金属度-粗糙度工作流的光照模型
- 车辆（metallic=0.7, roughness=0.35）与猫/植被（non-metallic）使用不同材质参数
- 支持 exposure 曝光控制

### 植被系统
- **Instanced 渲染**：14 种树木 + 6 种花草模型，GPU 实例化批量绘制
- **LOD 切换**：远距离植被自动简化
- **风动画**：着色器中基于时间的顶点摆动
- **湖泊避让**：自动移除水面区域的植被实例

### 角色与交互
- **猫的 AI 漫步**：10 只大猫在地图上随机散步（IDLE / RUN 状态机），1 只小猫在花丛中
- **骨骼动画**：基于 glTF 蒙皮动画，支持行走/奔跑/待机切换
- **碰撞检测**：AABB 碰撞系统，赛车与猫、树干、地图边界、湖泊进行碰撞响应
- **贴地行驶**：车辆根据地形高度和法线自动调整姿态

### 天空盒
- 6 面立方体贴图天空盒，在所有不透明物体之后绘制

## 操作方式

| 按键 | 功能 |
|------|------|
| `W` / `S` | 赛车前进 / 后退（驾驶模式）或摄像机前进 / 后退（自由模式） |
| `A` / `D` | 赛车左转 / 右转（驾驶模式）或摄像机平移（自由模式） |
| `Q` / `E` | 自由模式下降低 / 升高视角高度 |
| `C` | 切换驾驶模式 / 自由漫游模式 |
| `Alt` | 锁定 / 解锁鼠标 |
| `鼠标移动` | 旋转视角（鼠标锁定时） |
| `滚轮` | 沿视线方向前后移动摄像机 |
| `ESC` | 退出 |

## 构建方法

### 环境要求

- **CMake** >= 3.15
- **Visual Studio 2022**（MSVC v143，x64）
- **OpenGL 3.3+** 兼容显卡

所有第三方依赖（Assimp、GLFW、GLEW、GLM、ImGui、GLAD）均已包含在仓库中，无需额外下载。

### 构建步骤

```bash
git clone https://github.com/SHLINOvO/CatValley.git
cd CatValley
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

构建完成后，可执行文件及所有资源位于 `build/bin/Release/` 目录下，直接运行即可。

### 注意事项

- 项目路径建议不含中文或空格，避免部分工具链的路径问题
- Assimp 从源码编译，首次构建需要较长时间（后续增量编译很快）
- 如果运行时缺少 `MSVCP140.dll` 等运行时库，安装 [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)

## 项目结构

```
CatValley/
├── CMakeLists.txt              # CMake 构建配置
├── src/
│   ├── main.cpp                # 主程序入口（渲染循环、场景组装）
│   ├── glad.c                  # OpenGL 函数加载
│   ├── include/
│   │   ├── camera.hpp          # 摄像机（FPS + 第三人称跟随）
│   │   ├── shader.hpp          # Shader 编译/链接/_uniform 管理
│   │   ├── model.hpp           # 静态模型加载（glTF/OBJ）
│   │   ├── ani.hpp             # 骨骼动画模型（glTF 蒙皮）
│   │   ├── car.hpp             # 赛车物理（速度、转向、贴地）
│   │   ├── collision.hpp       # AABB 碰撞系统
│   │   ├── skybox.hpp          # 天空盒
│   │   ├── stb_image.h         # 图像加载
│   │   ├── terrain/            # 地形系统（LOD、Chunk、裁剪、高度图）
│   │   ├── vegetation/         # 植被管理（实例化、LOD、风动画）
│   │   └── water/              # 水面网格
│   ├── shaders/
│   │   ├── model.vs/fs         # 静态模型 PBR 着色器
│   │   ├── ani.vs              # 骨骼动画顶点着色器
│   │   ├── terrain.vs/fs       # 地形着色器（双纹理混合 + 阴影）
│   │   ├── tree.vs/fs          # 植被着色器（风动画 + 阴影）
│   │   ├── water.vs/fs         # 水面着色器（反射 + 波浪 + 菲涅尔）
│   │   ├── shadow_depth.vs/fs  # 阴影深度着色器
│   │   └── skybox.vs/fs        # 天空盒着色器
│   └── assets/
│       ├── car/                # 赛车模型（McLaren MCL39 + McLaren P1 GTR）
│       ├── munchkin_cat*/      # 曼基康猫模型（glTF + 蒙皮动画）
│       ├── cat/                # 备用猫模型（OBJ）
│       ├── terrain/            # 高度图 + 草地纹理 + 噪声图
│       ├── vegetation/         # 树木 + 花草模型（glb）
│       └── skybox/             # 天空盒 6 面贴图
├── glew-2.3.1/                 # GLEW 预编译静态库
├── glfw-3.4.bin.WIN64/         # GLFW 预编译二进制（vc2022）
└── include/                    # 第三方库源码
    ├── assimp/                 # Assimp（源码编译，OBJ/glTF/FBX 导入）
    ├── glm/                    # GLM 数学库
    ├── imgui-1.90.4/           # Dear ImGui
    ├── glad/                   # GLAD OpenGL loader
    └── KHR/                    # Khronos 平台头文件
```

## 技术栈

| 库 | 用途 | 版本 |
|----|------|------|
| OpenGL | 图形 API | 3.3 Core |
| GLFW | 窗口/输入管理 | 3.4 |
| GLEW | OpenGL 扩展加载 | 2.3.1 |
| GLM | 数学运算 | - |
| Assimp | 3D 模型导入 | 5.x（源码编译） |
| Dear ImGui | 调试 UI | 1.90.4 |
| GLAD | OpenGL 函数指针加载 | - |
| stb_image | 图像加载 | - |

## 渲染管线

每帧的渲染流程：

```
1. Shadow Pass
   └─ 绑定 Shadow FBO → 渲染深度到 2048² 阴影贴图
      （开启 Polygon Offset 消除 Shadow Acne）

2. Reflection Pass
   └─ 绑定 Reflect FBO → 镜像摄像机渲染场景到反射纹理
      （开启 Clip Plane 裁剪水面以上内容）

3. Normal Pass
   └─ 绑定默认 FBO → 摄像机视角渲染完整场景
      ├─ 猫（骨骼动画 + 阴影）
      ├─ 赛车（PBR + 阴影 + 贴地姿态）
      ├─ 地形（LOD + 双纹理混合 + 阴影）
      ├─ 植被（实例化 + 风动画 + 阴影）
      └─ 天空盒

4. Water Pass
   └─ 半透明渲染水面（反射纹理采样 + 波浪 + 菲涅尔）
      （关闭深度写入，Alpha 混合）
```

## License

本项目为课程作业，仅供学习交流。
