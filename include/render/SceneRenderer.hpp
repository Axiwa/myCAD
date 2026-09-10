#pragma once
#include <vector>
#include <optional>
#include <string>
#include <stack>

#include <glad/glad.h>
#include "render/GpuMesh.hpp"
#include "render/ShaderProgram.hpp"
#include "view/OrbitCamera.hpp"
#include "render/PickingFrameBuffer.hpp"
#include "profiler/profiler.hpp"
#include "scene/CadScene.hpp"
#include <filesystem>

class SceneRenderer {
public:
    SceneRenderer(const std::filesystem::path& executableDirectory);
    void collectObjects(CadScene& cadScene);
    void renderScene(OrbitCamera* const myCamera, CadScene& cadScene, SceneRuntimeState& state, RenderStats& renderStats);
    bool offscreenPicking(OrbitCamera* const myCamera, PickingFrameBuffer* const pickBuffer, GLFWwindow* myWindow, CadScene& cadScene, std::unordered_map<SceneSelection, PickId, SceneSelectionHash>& pickMap);

private:
    std::vector<std::vector<std::optional<GpuMesh>>> meshBuffers;
    ShaderProgram pickFirstProgram;
    ShaderProgram shaderProgram;
};