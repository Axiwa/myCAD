#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

#include "render/SceneRenderer.hpp"
#include "scene/CADScene.hpp"
#include "view/OrbitCamera.hpp"
#include "render/GpuMesh.hpp"
#include "render/PickingFrameBuffer.hpp"
#include "render/ShaderProgram.hpp"
#include "profiler/profiler.hpp"

extern bool enableInstance;
extern bool enableCulling;

SceneRenderer::SceneRenderer(const std::filesystem::path& executableDirectory){
    std::string vertpath = executableDirectory.string() + "/shader/plain.vert";
    std::string vertinstancepath = executableDirectory.string() + "/shader/plain_instance.vert";
    std::string fragpath = executableDirectory.string() + "/shader/plain.frag";
    std::string pickpath = executableDirectory.string() + "/shader/offscreen.frag";
    std::string draftpath = executableDirectory.string() + "/shader/draft.frag";

    if (!pickFirstProgram.LoadShader(vertpath, pickpath)) {
        std::cerr << "Load Shader " << vertpath << " " << pickpath << "Failed!";
    }
    if (!shaderProgram.LoadShader(vertpath, draftpath)){
        std::cerr << "Load Shader " << vertpath << " " << draftpath << "Failed!";
    }
    if (!instanceProgram.LoadShader(vertinstancepath, draftpath)){
        std::cerr << "Load Shader " << vertinstancepath << " " << draftpath << "Failed!";
    }
}

void SceneRenderer::collectObjects(CadScene& cadScene){
    meshBuffers.clear();
    meshBuffers.resize(cadScene.docs.size());
    for (int i = 0; i < cadScene.docs.size(); i++) {
        meshBuffers[i].resize(cadScene.docs[i].prototypes.size());
        for (int j = 0; j < meshBuffers[i].size(); j++) {
            auto& mesh = cadScene.docs[i].prototypes[j].mesh;
            if (cadScene.docs[i].prototypes[j].meshStatus == MeshStatus::Ready){
                meshBuffers[i][j] = GpuMesh();
                meshBuffers[i][j].value().upload(mesh);
            }
        }
    }
}

void SceneRenderer::renderScene(OrbitCamera* const myCamera, CadScene& cadScene, SceneRuntimeState& state, RenderStats& renderStats, const DraftAnalysisSettings& draftSetting){
    drawableItems.clear();
    if (state.sceneStructureDirty || state.visibleBoundsDirty){
        renderItems.clear();
    }

    std::optional<RenderItem> selected;

    // Draw cadScene
    for (int i = 0; i < cadScene.docs.size(); i++) {
        const auto& doc = cadScene.docs[i];
        for (const auto root : doc.roots) {
            std::stack<const SceneNode*> m_stack;
            m_stack.push(root);
                            
            while (!m_stack.empty()) {
                const SceneNode* curr = m_stack.top();
                m_stack.pop();
                if (curr->isVisible == false) {
                    continue;
                }

                if (curr->prototypeIndex.has_value()) {
                    auto index = curr->prototypeIndex.value();

                    if (index < meshBuffers[i].size()){
                        auto& mesh = doc.prototypes[index].mesh;
                        if (meshBuffers[i][index].has_value()){
                            glm::mat4 model = curr->worldTransform;
                            const bool mirrored = glm::determinant(glm::dmat3(model)) < 0.0;

                            if (state.selection.has_value() && state.selection->documentIndex == i && state.selection->nodeId == curr->id) {
                                glm::vec4 col = glm::vec4(1, 1, 0, 1);
                                selected.emplace();
                                InstanceKey key{ (size_t)i, index, col, mirrored };
                                selected->key = key;
                                selected->worldTransform = model;
                                selected->nodebbox = curr->worldBbox;
                            }
                            
                            // update scene bbox
                            if (state.sceneStructureDirty || state.visibleBoundsDirty) {
                                Bbox nodebox = curr->worldBbox; // bbox has been updated
                                state.visibleBounds.expand(nodebox.minimum);
                                state.visibleBounds.expand(nodebox.maximum);

                                InstanceKey key {(size_t)i, index, curr->colorStyle.surfaceColor, mirrored};
                                RenderItem item {key, curr->worldTransform, nodebox};
                                renderItems.emplace_back(item);
                            }

                            renderStats.visibleOccurrences++;
                        }
                    }
                }
                
                for (auto it = curr->children.rbegin(); it != curr->children.rend(); it++) {
                    m_stack.push(*it);
                }
            }
        }
    }

    if (state.sceneStructureDirty || state.visibleBoundsDirty || !wasInstancing){
        std::sort(renderItems.begin(), renderItems.end(), [](const RenderItem& a, const RenderItem& b){
            return a.key < b.key;
        });
    }

    bool isChanged = myCamera->calculateFrustum();
    if (enableCulling){
        for(auto& item: renderItems){
            if (myCamera->inFrustum(item.nodebbox)){
                drawableItems.emplace_back(item);
            }
            else{
                renderStats.culledOccurrences++;
            }
        }
    }
    else{
        drawableItems = renderItems;
    }

    if (enableInstance){
        instanceProgram.use();
        instanceProgram.setMat4("view", myCamera->viewMatrix());
        instanceProgram.setMat4("proj", myCamera->projectionMatrix());
        instanceProgram.setInt("analyzeDraft", draftSetting.enabled ? 1 : 0);
        if (draftSetting.enabled){
            instanceProgram.setVec3("pullDir", glm::vec3(draftSetting.pullDirection[0], draftSetting.pullDirection[1], draftSetting.pullDirection[2]));
            instanceProgram.setFloat("draftThreshold", draftSetting.requiredAngleDegrees);
        }

        if (state.sceneStructureDirty || state.visibleBoundsDirty || !wasInstancing || isChanged) {
            renderBatches.clear();

            int left = 0, right = 0;
            while (left < drawableItems.size()) {
                std::vector<glm::mat4> mats;
                mats.push_back(drawableItems[right].worldTransform);
                while (right < drawableItems.size() - 1 && drawableItems[right].key == drawableItems[right + 1].key) {
                    right++;
                    mats.push_back(drawableItems[right].worldTransform);
                }

                RenderBatch batch;
                batch.key = drawableItems[right].key;
                batch.instanceCount = right - left + 1;
                batch.uploadData(mats);

                renderBatches.emplace_back(std::move(batch));

                left = right + 1;
                right = left;
            }
        }

        for (auto& batch : renderBatches) {
            size_t documentIndex = batch.key.documentIndex;
            size_t prototypeIndex = batch.key.prototypeIndex;

            meshBuffers[documentIndex][prototypeIndex].value().setInstance(batch.instanceVBO);

            bool mirrored = batch.key.isMirrored;
            glFrontFace(mirrored ? GL_CW : GL_CCW);
            instanceProgram.setVec4("baseColor", batch.key.color);

            // Bind VAOInstanced and Upload data
            meshBuffers[documentIndex][prototypeIndex].value().drawInstance(batch.instanceCount);

            auto& mesh = cadScene.docs[documentIndex].prototypes[prototypeIndex].mesh;
            renderStats.drawCalls++;
            renderStats.submittedTriangles += (mesh.indices.size() / 3) * batch.instanceCount;
        }
    }
    else{
        shaderProgram.use();
        shaderProgram.setMat4("view", myCamera->viewMatrix());
        shaderProgram.setMat4("proj", myCamera->projectionMatrix());
        shaderProgram.setInt("analyzeDraft", draftSetting.enabled ? 1 : 0);
        if (draftSetting.enabled){
            shaderProgram.setVec3("pullDir", glm::vec3(draftSetting.pullDirection[0], draftSetting.pullDirection[1], draftSetting.pullDirection[2]));
            shaderProgram.setFloat("draftThreshold", draftSetting.requiredAngleDegrees);
        }

        for(auto& item: drawableItems){
            size_t documentIndex = item.key.documentIndex;
            size_t prototypeIndex = item.key.prototypeIndex;
            bool mirrored = item.key.isMirrored;
            glFrontFace(mirrored ? GL_CW : GL_CCW);

            shaderProgram.setMat4("model", item.worldTransform);
            shaderProgram.setVec4("baseColor", item.key.color);
            meshBuffers[documentIndex][prototypeIndex].value().draw();

            auto& mesh = cadScene.docs[documentIndex].prototypes[prototypeIndex].mesh;
            renderStats.drawCalls++;
            renderStats.submittedTriangles += mesh.indices.size() / 3;
        }
    }

    if (selected.has_value()){
        shaderProgram.use();
        shaderProgram.setMat4("view", myCamera->viewMatrix());
        shaderProgram.setMat4("proj", myCamera->projectionMatrix());
        shaderProgram.setInt("analyzeDraft", 0);

        auto& item = selected.value();
        if (!enableCulling || myCamera->inFrustum(item.nodebbox)) {
            size_t documentIndex = item.key.documentIndex;
            size_t prototypeIndex = item.key.prototypeIndex;
            bool mirrored = item.key.isMirrored;
            glFrontFace(mirrored ? GL_CW : GL_CCW);

            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
            shaderProgram.setMat4("model", item.worldTransform);
            shaderProgram.setVec4("baseColor", item.key.color);
            meshBuffers[documentIndex][prototypeIndex].value().draw();
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);

            auto& mesh = cadScene.docs[documentIndex].prototypes[prototypeIndex].mesh;
            renderStats.drawCalls++;
            renderStats.submittedTriangles += mesh.indices.size() / 3;
        }
    }
    wasInstancing = enableInstance;
}

bool SceneRenderer::offscreenPicking(OrbitCamera* const myCamera, PickingFrameBuffer* const pickBuffer, GLFWwindow* myWindow, CadScene& cadScene, std::unordered_map<SceneSelection, PickId, SceneSelectionHash>& pickMap){
    // =========== Draw picking offscreen ========
    GLint origFB;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &origFB);
    if (!pickBuffer->bind()){
        std::cout<<"Cannot bind picking buffer"<<std::endl;
        pickBuffer->unbind(origFB, myWindow);
        return false;
    }

    pickFirstProgram.use();
    pickFirstProgram.setMat4("view", myCamera->viewMatrix());
    pickFirstProgram.setMat4("proj", myCamera->projectionMatrix());
    for (int i = 0; i < cadScene.docs.size(); i++) {
        const auto& doc = cadScene.docs[i];
        for (const auto root : doc.roots) {
            std::stack<const SceneNode*> m_stack;
            m_stack.push(root);

            while (!m_stack.empty()) {
                const SceneNode* curr = m_stack.top();
                m_stack.pop();
                if (curr->isVisible == false) {
                    continue;
                }

                if (curr->prototypeIndex.has_value()) {
                    auto index = curr->prototypeIndex.value();

                    if (index < meshBuffers[i].size()) {
                        auto& mesh = doc.prototypes[index].mesh;
                        SceneSelection selection{ (size_t)i, curr->id };

                        if (meshBuffers[i][index].has_value() && pickMap.find(selection) != pickMap.end()) {
                            glm::mat4 model = curr->worldTransform;
                            pickFirstProgram.setMat4("model", model);
                            pickFirstProgram.setUint("pickId", pickMap[selection]);

                            const bool mirrored = glm::determinant(glm::dmat3(model)) < 0.0;
                            glFrontFace(mirrored ? GL_CW : GL_CCW);
                            meshBuffers[i][index].value().draw();
                        }
                    }
                }
                for (auto it = curr->children.rbegin(); it != curr->children.rend(); it++) {
                    m_stack.push(*it);
                }
            }
        }
    }

    pickBuffer->unbind(origFB, myWindow);
    return true;
}