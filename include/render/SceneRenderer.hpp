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
#include "geometry/Design.hpp"
#include <filesystem>

struct InstanceKey{
    size_t documentIndex;
    size_t prototypeIndex;
    glm::vec4 color;
    bool isMirrored;

    bool operator==(const InstanceKey& other) const {
        return documentIndex == other.documentIndex && prototypeIndex == other.prototypeIndex && color == other.color && isMirrored == other.isMirrored;
    }

    bool operator<(const InstanceKey& other) const {
        return std::tie(documentIndex, prototypeIndex, isMirrored, color.x, color.y, color.z, color.w) <
               std::tie(other.documentIndex, other.prototypeIndex, other.isMirrored, other.color.x, other.color.y, other.color.z, other.color.w);
    }
};

struct RenderItem{
    InstanceKey key;
    glm::mat4 worldTransform;
    Bbox nodebbox;
};

struct RenderBatch {
    GLuint instanceVBO;
    InstanceKey key;
    GLsizei instanceCount = 0;
    bool isDirty = false;

    RenderBatch(){
        glGenBuffers(1, &instanceVBO);
    }
    RenderBatch(RenderBatch&& other) noexcept:
    instanceVBO(std::exchange(other.instanceVBO, 0)),
    key(other.key),
    instanceCount(other.instanceCount),
    isDirty(other.isDirty)
    { }

    RenderBatch& operator=(RenderBatch&& other) noexcept {
        if (this != &other){
            if (instanceVBO != 0){
                glDeleteBuffers(1, &instanceVBO);
                instanceVBO = 0;
            }
            instanceVBO = std::exchange(other.instanceVBO, 0);
            key = other.key;
            instanceCount = other.instanceCount;
            isDirty = other.isDirty;
        }
        return *this;
    }
    RenderBatch(const RenderBatch& other) = delete;
    RenderBatch& operator=(const RenderBatch&) = delete;


    ~RenderBatch(){
        if (instanceVBO != 0){
            glDeleteBuffers(1, &instanceVBO);
            instanceVBO = 0;
        }
    }
    void uploadData(std::vector<glm::mat4>& mats){
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        // glBufferData(GL_ARRAY_BUFFER, mats.size() * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW); // orphan
        // glBufferSubData(GL_ARRAY_BUFFER, 0, mats.size() * sizeof(glm::mat4), mats.data());
        glBufferData(GL_ARRAY_BUFFER, mats.size() * sizeof(glm::mat4), mats.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
};

class SceneRenderer {
public:
    SceneRenderer(const std::filesystem::path& executableDirectory);
    void collectObjects(CadScene& cadScene);
    void renderScene(OrbitCamera* const myCamera, CadScene& cadScene, SceneRuntimeState& state, RenderStats& renderStats, const DraftAnalysisSettings& draftSetting);
    bool offscreenPicking(OrbitCamera* const myCamera, PickingFrameBuffer* const pickBuffer, GLFWwindow* myWindow, CadScene& cadScene, std::unordered_map<SceneSelection, PickId, SceneSelectionHash>& pickMap);

private:
    bool wasInstancing = true;
    std::vector<std::vector<std::optional<GpuMesh>>> meshBuffers;
    std::vector<RenderItem> renderItems;
    std::vector<RenderBatch> renderBatches;
    std::vector<RenderItem> drawableItems;
    ShaderProgram pickFirstProgram;
    ShaderProgram shaderProgram;
    ShaderProgram instanceProgram;
};