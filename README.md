# Little CAD Viewer

基于 C++17 + OCCT 8.0.1 + OpenGL 3.3 实现的工业可视化项目。主要功能是 STEP 装配体的导入、三角化和绘制，未使用OCCT自带的可视化功能。使用 XCAF 进行模型的导入，导入过程中建立该文件的多叉树并储存prototype的mesh信息。使用 ImGUI 展示渲染信息和场景层级，支持点击选择高亮和控制显隐。使用 OrbitCamera 进行视角的交互，包括旋转、平移、缩放，将选中物体放到中心。绘制方面考虑了零件的镜像情况以及各零件在文件中自带的颜色。优化方面实现了 Frustum culling 和 Instancing，在测试模型高达65542个时，打开instancing绘制时间从24ms降到12ms。利用 fragment shader 做了一个简单的拔模角分析。

<p align="center">
  <video src="https://github.com/Axiwa/myCAD/blob/main/docs/myCAD.mp4" width="80%" controls></video>
</p>

## 构建环境准备

### 依赖

| 依赖 | 位置 | 来源 |
|---|---|---|
| OpenCASCADE 8.0.1 (release + debug, vc14 x64) | `External/opencascade-8.0.1-vc14-64` | 脚本下载，**不入库** |
| OCCT 3rdparty bundle (vc14 x64) | `External/3rdparty-vc14-64` | 脚本下载，**不入库** |
| GLFW 3.4 | `External/glfw-3.4` | 随仓库提交 |
| Dear ImGui | `External/imgui` | 随仓库提交 |

OCCT 及其 3rdparty 合计约 1.6 GB，为避免仓库膨胀，它们不再提交到 git，改用脚本获取。

### 首次构建

```powershell
# 1. 下载并校验第三方预编译包（约 630 MB 下载，解压后 1.6 GB）
powershell -ExecutionPolicy Bypass -File scripts\fetch-external.ps1

# 2. 配置（注意：PATH 里的 Strawberry cmake 较旧，需用官方 CMake）
& 'C:\Program Files\CMake\bin\cmake.exe' -S . -B build

# 3. 编译
& 'C:\Program Files\CMake\bin\cmake.exe' --build build --config Debug
& 'C:\Program Files\CMake\bin\cmake.exe' --build build --config Release
```

脚本把下载包缓存在 `%LOCALAPPDATA%\myCAD\external-cache`，重复执行会跳过已存在的目录；需要强制刷新时加 `-Force`，跳过哈希校验加 `-SkipHashCheck`。

### 运行

编译后 `build/Debug`、`build/Release` 都是自包含目录（OCCT / 3rdparty / GLFW 的 DLL、`shader/`、`fonts/`、`data/step/` 都由 POST_BUILD 拷入），直接运行即可：

```powershell
.\build\Debug\helloCAD.exe            # 默认加载 data\step\linkrods.step
.\build\Debug\helloCAD.exe model.step # 指定 STEP 文件
```

### 说明

如果要更换依赖版本，改 `scripts/fetch-external.ps1` 顶部的 `$packages` 表（Url 与 Sha256）以及 `CMakeLists.txt` 里的版本号。