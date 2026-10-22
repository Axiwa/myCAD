

## Codely Structured Memories

### User

### Feedback

### Project
- [2026-10-20 14:47:58] 本项目构建必须用 C:\Program Files\CMake\bin\cmake.exe（4.3.1）；PATH 中的 cmake 是 Strawberry 自带 3.29.2，不识别已有的 "Visual Studio 18 2026" 生成器，直接调用会报生成器无效。
- [2026-10-20 14:47:58] OCCT 8.0.1 二进制包位于 External/opencascade-8.0.1-vc14-64（win64/vc14: bin+bind、lib+libd），3rdparty 位于 External/3rdparty-vc14-64；OCCT imported target 依赖 TBB::tbb/TBB::tbbmalloc（来自 tbb-2021.13.0-x64/lib/cmake/TBB），运行期还需按配置拷贝 tbb12(_debug)、jemalloc、FreeImage、freetype、ffmpeg、openvr_api(bin/win64) 等 DLL。
- [2026-10-22 10:20:19] 2026-10-20 起 External/opencascade-8.0.1-vc14-64 与 External/3rdparty-vc14-64 已从 git 移除并被 .gitignore 忽略（原 1.6GB，含 652MB PDB），首次构建需先跑 scripts/fetch-external.ps1（固定 OCCT V8.0.1 release 包 URL + SHA256，缓存到 %LOCALAPPDATA%\myCAD\external-cache）；*.pdb 不再入库、LFS 规则已删除；glfw-3.4 与 imgui 仍随仓库提交；CMakeLists 在 OCCT 缺失时会 FATAL_ERROR 提示该脚本。说明见 docs/setup.md。历史中的旧二进制 blob 未清理（.git pack 仍约 423MB）。

### Reference

