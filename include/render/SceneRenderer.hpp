#pragma once
#include <vector>
#include <optional>
#include <string>
#include <stack>

#include <glad/glad.h>
#include "render/GpuMesh.hpp"
#include "view/Controller.hpp"
#include "render/PickingFrameBuffer.hpp"

extern std::filesystem::path executableDirectory();

class SceneRenderer {
public:
    SceneRenderer(){
        std::string vertpath = executableDirectory().string() + "/shader/plain.vert";
        std::string fragpath = executableDirectory().string() + "/shader/plain.frag";
        std::string pickpath = executableDirectory().string() + "/shader/offscreen.frag";
        std::string highlightpath = executableDirectory().string() + "/shader/highlight.frag";

        if (!pickFirstProgram.LoadShader(vertpath, pickpath)) {
            std::cerr << "Load Shader " << vertpath << " " << pickpath << "Failed!";
        }
        if (!shaderProgram.LoadShader(vertpath, fragpath)){
            std::cerr << "Load Shader " << vertpath << " " << fragpath << "Failed!";
        }
        if (!pickSecondProgram.LoadShader(vertpath, highlightpath)) {
            std::cerr << "Load Shader " << vertpath << " " << highlightpath << "Failed!";
        }
    }

    void collectObjects(CadScene& cadScene){
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

    void renderScene(Controller& myWindow, CadScene& cadScene, SceneRuntimeState& state){
        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shaderProgram.use();
        shaderProgram.setMat4("view", myWindow.GetCamera()->viewMatrix());
        shaderProgram.setMat4("proj", myWindow.GetCamera()->projectionMatrix());

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
                                shaderProgram.setMat4("model", model);
                                if (state.selection.has_value() && state.selection->documentIndex == i && state.selection->nodeId == curr->id) {
                                    glm::vec4 col = glm::vec4(1, 1, 0, 1);
                                    shaderProgram.setVec4("baseColor", col);
                                }
                                else {
                                    shaderProgram.setVec4("baseColor", curr->colorStyle.surfaceColor);
                                }
                                
                                // update scene bbox
                                if (state.visibleBoundsDirty) {
                                    Bbox nodebox = doc.prototypes[index].localBbox;
                                    nodebox.applyTransform(model);
                                    state.visibleBounds.expand(nodebox.minimum);
                                    state.visibleBounds.expand(nodebox.maximum);
                                }

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
    }

    void offscreenPicking(Controller& myWindow, CadScene& cadScene, std::unordered_map<SceneSelection, PickId, SceneSelectionHash>& pickMap){
        // =========== Draw picking offscreen ========
        GLint origFB;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &origFB);
        myWindow.bindPickBuffer();

        pickFirstProgram.use();
        pickFirstProgram.setMat4("view", myWindow.GetCamera()->viewMatrix());
        pickFirstProgram.setMat4("proj", myWindow.GetCamera()->projectionMatrix());
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

        myWindow.unbindPickBuffer(origFB);
        return;
    }

private:
    std::vector<std::vector<std::optional<GpuMesh>>> meshBuffers;
    ShaderProgram pickFirstProgram;
    ShaderProgram shaderProgram;
    ShaderProgram pickSecondProgram;
};