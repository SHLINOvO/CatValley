# CGFinal
## 0 环境配置
现已配置glfw,glew,glm,imgui,assimp库

把仓库克隆下来然后在目录的命令行中运行：
```
mkdir build
cd build
cmake ..
cmake --build .
```
之后在vs studio打开项目就可以，接着运行`main.cpp`测试库是不是都装好了

*PS.assimp是在仓库直接克隆的源码；其他是下载的zip包解压就好；glm是助教给的zip包
```
在include文件夹里git clone https://github.com/assimp/assimp.git
https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip
https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
https://github.com/ocornut/imgui/archive/refs/tags/v1.90.4.zip (imgui)
```

*PS.PS. 尽量在全英文路径下建立项目，不然环境有时候抽风找不到包（被折磨致死精神错乱版）

## 1 统一规范
- 注释用中文写，每个函数前加一行注释说明它的用处
- 3D模型尽量用`glb+png贴图`或者`.obj+.mtl+diffuse.jpg`的形式来
- 在`include/config.hpp`里可以改根目录`PROJECT_ROOT`，其他的路径应该就不用改了

## 2 函数的使用

### 🌟 主函数（main.cpp）

### 2.0 全局变量
```
// 屏幕长宽
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// 相机位置
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

// 渲染间隔时间（之类的吧）
float deltaTime = 0.0f;
float lastFrame = 0.0f;
```

### 2.1 天空盒
**天空盒要在所有3D模型画完之后再画，不然会挡住3D模型！！**（参考`skybox.draw(view, projection);`的位置，把它放在主函数的渲染循环的最后即可）

- 初始化

**在主函数的渲染循环前完成**(即`while (!glfwWindowShouldClose(window)){}`之前)

主要是导入天空盒的贴图
```
   // 初始化、加载天空盒
    std::vector<std::string> faces = {
        PROJECT_ROOT + "/src/assets/skybox/right.jpg",
        PROJECT_ROOT + "/src/assets/skybox/left.jpg",
        PROJECT_ROOT + "/src/assets/skybox/top.jpg",
        PROJECT_ROOT + "/src/assets/skybox/bottom.jpg",
        PROJECT_ROOT + "/src/assets/skybox/front.jpg",
        PROJECT_ROOT + "/src/assets/skybox/back.jpg"
    };
    Skybox skybox(faces);
```

### 2.2 导入3D模型与初始化

- 初始化、加载模型： 

**在主函数的渲染循环前完成**(即`while (!glfwWindowShouldClose(window)){}`之前)

`glb`的要准备纹理索引表`texturemap`
```c
    // 准备赛车贴图
    std::map<std::string, std::string> carTextureMap = {
        {"*0", "2015_mclaren_p1_gtr_wheel.etc_0.png"},
        {"*1", "car_tyre_slick_pirelli_02.etc_1.png"},
        {"*2", "2015_mclaren_p1_gtr_misc.etc_2.png"},
        {"*3", "car_rotor_03.etc_3.png"},
        {"*4", "car_windows.etc_4.png"},
        {"*5", "2015_mclaren_p1_gtr_ext_51.etc_5.png"},
        {"*6", "2015_mclaren_p1_gtr_cab.etc_6.png"},
        {"*7", "2015_mclaren_p1_gtr_lights.etc_7.png"},
        {"*8", "2015_mclaren_p1_gtr_badges.etc_8.png"},
        {"*9", "car_chassis.etc_9.png"}
    };
    // 初始化.(3D模型路径，纹理文件夹路径，纹理索引表)
    Model mclaren((ASSETS_FOLDER+"car/2015 McLaren P1 GTR.glb").c_str(), (ASSETS_FOLDER + "car/texture").c_str(), carTextureMap);
```
`obj`的会自动从`mtl`文件读纹理图片所以textureMap可以为空
```c
    // 创建一个空的cattextureMap就行
    std::map<std::string, std::string> cattextureMap;
    // 初始化.(3D模型路径，纹理文件夹路径，纹理索引表-空的就行)
    Model cat1((ASSETS_FOLDER+"cat/Cat1/12221_Cat_v1_l3.obj").c_str(), (ASSETS_FOLDER + "cat/Cat1/").c_str(),cattextureMap);
```
- 导入3D模型

模型可以共享的设置。如光照位置和光照颜色
```c
// ------------模型可以共享的设置-------------
        // 设置 MVP
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        // 光照
        ourShader.setVec3("viewPos", camera.Position);
        ourShader.setVec3("light.position", glm::vec3(0.0f, 1.0f, 10.0f));
        ourShader.setVec3("light.color", glm::vec3(1.0f, 1.0f, 1.0f));
```
每个模型的个性化设置（平移、旋转、缩放等），以猫的模型为例
```c
        // ------------画猫的模型---------------------
        // 设置模型矩阵
        glm::mat4 modelCat = glm::mat4(1.0f);
        // 先旋转后平移
        modelCat = glm::rotate(modelCat, glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
        // 平移因子
        modelCat = glm::translate(modelCat, glm::vec3(-2.0f, -0.5f, -5.0f)); // 放在左边
        // 缩放因子
        modelCat = glm::scale(modelCat, glm::vec3(0.2f, 0.2f, 0.2f));
        ourShader.setMat4("model", modelCat);
        cat1.Draw(ourShader);
```
- 3D模型的光照

要找它的shader的`fs`文件(`shaders/model.fs`，目前是加了环境光漫反射和高光的
```c
// shaders/model.fs
void main() {
    // 1. 从纹理获取基础颜色（漫反射底色）
    vec3 textureColor = texture(texture_diffuse1, TexCoords).rgb;

    // 2. 环境光
    vec3 ambient = 0.1 * light.color * textureColor;

    // 3. 漫反射
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * light.color * textureColor;

    // 4. 镜面反射（通常不乘纹理颜色，因为高光是光源属性）
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
    vec3 specular = spec * light.color; // 注意：这里没乘 textureColor

    // 5. 合成最终颜色
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);

}
```

### 🌟Shader

`Shader`类目前好像没啥要改的。

反正每次初始化它都要准备`fs`和`vs`文件。目前是天空盒一个shader,猫和车用一个shader（感觉后边要分开）

## 3 TODO
- 花园的植物景观要布局、然后导入，天空盒要换 → 让花花草草动起来（风） → 优化光照
- 赛车的光照 → 赛车的移动
- 猫的光照 → 猫的尾巴动 → 猫的爪子动 → 猫猫转头