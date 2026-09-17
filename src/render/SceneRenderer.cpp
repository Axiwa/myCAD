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

SceneRenderer::SceneRenderer(const std::filesystem::path& executableDirectory){
    std::string vertpath = executableDirectory.string() + "/shader/plain.vert";
    std::string vertinstancepath = executableDirectory.string() + "/shader/plain_instance.vert";
    std::string fragpath = executableDirectory.string() + "/shader/plain.frag";
    std::string pickpath = executableDirectory.string() + "/shader/offscreen.frag";

    if (!pickFirstProgram.LoadShader(vertpath, pickpath)) {
        std::cerr << "Load Shader " << vertpath << " " << pickpath << "Failed!";
    }
    if (!shaderProgram.LoadShader(vertpath, fragpath)){
        std::cerr << "Load Shader " << vertpath << " " << fragpath << "Failed!";
    }
    if (!instanceProgram.LoadShader(vertinstancepath, fragpath)){
        std::cerr << "Load Shader " << vertinstancepath << " " << fragpath << "Failed!";
    }
}

void SceneRenderer::collectObjects(CadScene& cadScene){
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

void SceneRenderer::renderScene(OrbitCamera* const myCamera, CadScene& cadScene, SceneRuntimeState& state, RenderStats& renderStats){
    if (state.sceneStructureDirty || state.visibleBoundsDirty){
        renderItems.clear();
    }
    std::optional<RenderItem> selected;

    // Draw cadScene
    for (int i = 0; i < cadScene.docs.size(); i++) {
        const auto& doc = cadScene.docs[i];
        for (const auto& root : doc.roots) {
            std::stack<const SceneNode*> m_stack;
            m_stack.push(&root);
                            
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
                            glm::vec4 col = curr->colorStyle.surfaceColor;

                            if (state.selection.has_value() && state.selection->documentIndex == i && state.selection->nodeId == curr->id) {
                                col = glm::vec4(1, 1, 0, 1);
                                selected.emplace();
                                InstanceKey key{ i, index, col, mirrored };
                                selected->key = key;
                                selected->worldTransform = model;
                            }
                            
                            // update scene bbox
                            if (state.sceneStructureDirty || state.visibleBoundsDirty) {
                                Bbox nodebox = doc.prototypes[index].localBbox;
                                nodebox.applyTransform(model);
                                state.visibleBounds.expand(nodebox.minimum);
                                state.visibleBounds.expand(nodebox.maximum);

                                InstanceKey key {i, index, col, mirrored};
                                RenderItem item {key, curr->worldTransform};
                                renderItems.emplace_back(item);
                            }

                            renderStats.visibleOccurrences++;
                        }
                    }
                }
                
                for (auto it = curr->children.rbegin(); it != curr->children.rend(); it++) {
                    m_stack.push(&(*it));
                }
            }
        }
    }

    if (enableInstance){
        if (state.sceneStructureDirty || state.visibleBoundsDirty){
            std::sort(renderItems.begin(), renderItems.end(), [](const RenderItem& a, const RenderItem& b){
                if (a.key.documentIndex < b.key.documentIndex) { return true; }
                else if (a.key.documentIndex > b.key.documentIndex) { return false; }
                if (a.key.prototypeIndex < b.key.prototypeIndex) {return true;}
                else if (a.key.prototypeIndex > b.key.prototypeIndex) {return false;}
                if (a.key.isMirrored == false && b.key.isMirrored == true) {return true;}
                else if (a.key.isMirrored == false && b.key.isMirrored == false) {return false;}

                if (a.key.color.x < b.key.color.x) {return true; }
                else if (a.key.color.x > b.key.color.x) {return false; }

                if (a.key.color.y < b.key.color.y) {return true; }
                else if (a.key.color.y > b.key.color.y) {return false; }

                if (a.key.color.z < b.key.color.z) {return true; }
                else if (a.key.color.z > b.key.color.z) {return false; }

                if (a.key.color.w < b.key.color.w) {return true; }
                else if (a.key.color.w > b.key.color.w) {return false; }
            });
        }

        instanceProgram.use();
        instanceProgram.setMat4("view", myCamera->viewMatrix());
        instanceProgram.setMat4("proj", myCamera->projectionMatrix());
        int left = 0, right = 0;
        while (left < renderItems.size()) {
            std::vector<glm::mat4> mats;
            mats.push_back(renderItems[right].worldTransform);
            while (right < renderItems.size()-1 && renderItems[right].key == renderItems[right+1].key) {
                right++;
                mats.push_back(renderItems[right].worldTransform);
            }
            // DRAW from left to right
            auto& item = renderItems[left];
            size_t documentIndex = item.key.documentIndex;
            size_t prototypeIndex = item.key.prototypeIndex;
            
            if (state.sceneStructureDirty || state.visibleBoundsDirty){
                // Upload data
                meshBuffers[documentIndex][prototypeIndex].value().uploadInstance(mats);
            }

            bool mirrored = item.key.isMirrored;
            glFrontFace(mirrored ? GL_CW : GL_CCW);

            instanceProgram.setVec4("baseColor", item.key.color);
            
            meshBuffers[documentIndex][prototypeIndex].value().drawInstance(mats.size());
            auto& mesh = cadScene.docs[documentIndex].prototypes[prototypeIndex].mesh;
            renderStats.drawCalls++;
            renderStats.submittedTriangles += mesh.indices.size() / 3;

            left = right + 1;
            right = left;
        }
    }
    else{
        shaderProgram.use();
        shaderProgram.setMat4("view", myCamera->viewMatrix());
        shaderProgram.setMat4("proj", myCamera->projectionMatrix());
        for(auto& item: renderItems){
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

        auto& item = selected.value();

        size_t documentIndex = item.key.documentIndex;
        size_t prototypeIndex = item.key.prototypeIndex;
        bool mirrored = item.key.isMirrored;
        glFrontFace(mirrored ? GL_CW : GL_CCW);

        glDisable(GL_DEPTH_TEST);
        shaderProgram.setMat4("model", item.worldTransform);
        shaderProgram.setVec4("baseColor", item.key.color);
        meshBuffers[documentIndex][prototypeIndex].value().draw();
        glEnable(GL_DEPTH_TEST);
    }
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
        for (const auto& root : doc.roots) {
            std::stack<const SceneNode*> m_stack;
            m_stack.push(&root);

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
                        SceneSelection selection{ i, curr->id };

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
                    m_stack.push(&(*it));
                }
            }
        }
    }

    pickBuffer->unbind(origFB, myWindow);
    return true;
}