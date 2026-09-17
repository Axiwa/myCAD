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

struct InstanceKey{
    std::size_t documentIndex;
    std::size_t prototypeIndex;
    glm::vec4 color;
    bool isMirrored;

    bool operator==(const InstanceKey& other) const {
        return documentIndex == other.documentIndex && prototypeIndex == other.prototypeIndex && color == other.color && isMirrored == other.isMirrored;
    }
};

struct RenderItem{
    InstanceKey key;
    glm::mat4 worldTransform;
};

class SceneRenderer {
public:
    SceneRenderer(const std::filesystem::path& executableDirectory);
    void collectObjects(CadScene& cadScene);
    void renderScene(OrbitCamera* const myCamera, CadScene& cadScene, SceneRuntimeState& state, RenderStats& renderStats);
    bool offscreenPicking(OrbitCamera* const myCamera, PickingFrameBuffer* const pickBuffer, GLFWwindow* myWindow, CadScene& cadScene, std::unordered_map<SceneSelection, PickId, SceneSelectionHash>& pickMap);

private:
    std::vector<std::vector<std::optional<GpuMesh>>> meshBuffers;
    std::vector<RenderItem> renderItems;
    ShaderProgram pickFirstProgram;
    ShaderProgram shaderProgram;
    ShaderProgram instanceProgram;
};