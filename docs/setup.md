# 构建环境准备

## 依赖

| 依赖 | 位置 | 来源 |
|---|---|---|
| OpenCASCADE 8.0.1 (release + debug, vc14 x64) | `External/opencascade-8.0.1-vc14-64` | 脚本下载，**不入库** |
| OCCT 3rdparty bundle (vc14 x64) | `External/3rdparty-vc14-64` | 脚本下载，**不入库** |
| GLFW 3.4 | `External/glfw-3.4` | 随仓库提交 |
| Dear ImGui | `External/imgui` | 随仓库提交 |

OCCT 及其 3rdparty 合计约 1.6 GB，为避免仓库膨胀，它们不再提交到 git，改用脚本获取。

## 首次构建

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

## 运行

编译后 `build/Debug`、`build/Release` 都是自包含目录（OCCT / 3rdparty / GLFW 的 DLL、`shader/`、`fonts/`、`data/step/` 都由 POST_BUILD 拷入），直接运行即可：

```powershell
.\build\Debug\helloCAD.exe            # 默认加载 data\step\linkrods.step
.\build\Debug\helloCAD.exe model.step # 指定 STEP 文件
```

## 说明

- PDB（调试符号）不入库，`.gitignore` 已忽略 `*.pdb`。需要单步调试进 OCCT/TBB 时，重新运行脚本解压官方包即可得到 PDB。
- 如果要更换依赖版本，改 `scripts/fetch-external.ps1` 顶部的 `$packages` 表（Url 与 Sha256）以及 `CMakeLists.txt` 里的版本号。