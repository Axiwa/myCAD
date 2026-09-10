#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stack>

#include <Windows.h>
#include <string>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "scene/CADScene.hpp"
#include "view/Controller.hpp"
#include "geometry/MeshData.hpp"
#include "geometry/OcctMesher.hpp"
#include "render/ShaderProgram.hpp"
#include "render/GpuMesh.hpp"
#include "render/PickingFrameBuffer.hpp"
#include "render/SceneRenderer.hpp"
#include "asset/StepXcafImporter.hpp"
#include "ui/ScenePanel.hpp"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

bool isFaceMirror = false;
bool isNodeMirror = false;

std::filesystem::path executableDirectory()
{
    std::wstring buffer(32768, L'\0');

    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error(
            "Failed to get executable path"
        );
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

int main(int argc, char* argv[]) {
    CadScene cadScene;
    StepXcafImporter xcafImporter;
    std::vector<std::vector<SceneNode*>> runtimeNodes;

    bool needsInitialFit = true;

    CadDocument test;
    std::string path = "C:/Users/28025/Learn/model/chassis.step";
    if (argc > 1) {
        path = argv[1];
        std::cout << "Import path: " << path << std::endl;
    }
    else{
        std::cout<<"Null import path, use default: " << path <<std::endl;
    }

    if (!xcafImporter.load(path.c_str(), test)) { // "C:/Users/28025/Learn/Geometry/OCCT-8.0.1/data/step/linkrods.step"
        exit(1);
    }
    cadScene.docs.emplace_back(std::move(test));

    Controller myWindow;
    glfwMakeContextCurrent(myWindow.GetWindow());

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return 1;
    }

    myWindow.InitPickframebuffer();

    // IMGUI
    ScenePanel scenePanel(myWindow.GetWindow());
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Create mesh buffers for cadScene
    SceneRenderer sceneRenderer;
    sceneRenderer.collectObjects(cadScene);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    bool isPressed = false;
    bool wasPressed = false;

    SceneRuntimeState sceneState;
    cadScene.getRuntimeNodes(runtimeNodes);

    std::vector<SceneSelection> pickRegistry;
    std::unordered_map<SceneSelection, PickId, SceneSelectionHash> pickMap;
    for(int i = 0; i<runtimeNodes.size(); i++){
        auto& doc = runtimeNodes[i];
        for(auto node: doc){
            if (node && node->prototypeIndex.has_value()){
                auto index = node->prototypeIndex.value();
                if (cadScene.docs[i].prototypes[index].meshStatus == MeshStatus::Ready){
                    pickRegistry.emplace_back();
                    pickRegistry.back().documentIndex = i;
                    pickRegistry.back().nodeId = node->id;
                    pickMap[pickRegistry.back()] = pickRegistry.size();
                }
            }
        }
    }

    while(!glfwWindowShouldClose(myWindow.GetWindow())){
        glfwPollEvents();

        std::size_t numPrototype = 0;
        std::size_t numSceneNode = 0;
        std::size_t numTriangles = 0;

        PickId pickedID = 0;

        bool allowViewportMouse = !ImGui::GetIO().WantCaptureMouse;
        if (!myWindow.update(allowViewportMouse)) {
            glfwWaitEvents();
            continue;
        }

        // Start the Dear ImGui frame
        scenePanel.newFrame();

        int width, height;
        glfwGetFramebufferSize(myWindow.GetWindow(), &width, &height);

        if (sceneState.sceneStructureDirty){
            // this will not happen for now but ...
            cadScene.getRuntimeNodes(runtimeNodes);
            sceneState.sceneStructureDirty = false;
        }

        std::optional<mouseCoord> pixelCoord;
        bool leftPress = myWindow.consumeLeftPress(pixelCoord);
        if (allowViewportMouse && leftPress && pixelCoord.has_value()){
            sceneRenderer.offscreenPicking(myWindow, cadScene, pickMap);
            pickedID = myWindow.GetPickFramebuffer()->pickID(pixelCoord->x, pixelCoord->y);
            std::cout << "Clicked ID: " << pickedID << std::endl;
        }

        if (pickedID > 0 && pickedID <= pickRegistry.size()) {
            sceneState.selection = pickRegistry[pickedID-1];
        }
        else {
            if (pickedID > pickRegistry.size()) {
                std::cout << "Pick something not pickable!" << std::endl;
            }
            pickedID = 0;
        }

        if (sceneState.visibleBoundsDirty) {
            sceneState.visibleBounds.reset();
        }
        sceneRenderer.renderScene(myWindow, cadScene, sceneState);

        if (needsInitialFit && sceneState.visibleBounds.valid()){
            myWindow.GetCamera()->fitToBbox(sceneState.visibleBounds);
            needsInitialFit = false;
        }
        sceneState.visibleBoundsDirty = false;

        isPressed = (glfwGetKey(myWindow.GetWindow(), GLFW_KEY_F) == GLFW_PRESS);
        if (isPressed && !wasPressed) {
            if (sceneState.selection.has_value()){
                Bbox rootbox;
                int docindex = sceneState.selection->documentIndex;
                int nodeid = sceneState.selection->nodeId;
                if (docindex < runtimeNodes.size() && nodeid < runtimeNodes[docindex].size()) {
                    SceneNode* root = cadScene.resolveSelection(sceneState.selection.value(), runtimeNodes);
                    std::stack<const SceneNode*> m_stack;
                    if (root) m_stack.push(root);
                    while (!m_stack.empty()) {
                        const SceneNode* curr = m_stack.top();
                        m_stack.pop();
                        if (!curr->isVisible){
                            continue;
                        }

                        if (curr->prototypeIndex.has_value()) {
                            int prototypeid = curr->prototypeIndex.value();
                            if (prototypeid >= cadScene.docs[docindex].prototypes.size()){
                                continue;
                            }
                            auto prototype = cadScene.docs[docindex].prototypes[prototypeid];
                            if (prototype.meshStatus != MeshStatus::Ready ||
                                !prototype.localBbox.valid()) {
                                continue;
                            }
                            glm::mat4 model = curr->worldTransform;
                            Bbox nodebox = prototype.localBbox;
                            nodebox.applyTransform(model);
                            rootbox.expand(nodebox.minimum);
                            rootbox.expand(nodebox.maximum);
                        }

                        for (auto it = curr->children.rbegin(); it != curr->children.rend(); it++) {
                            m_stack.push(&(*it));
                        }
                    }
                }
                else {
                    std::cerr << "Select something wrong!" << std::endl;
                }
                myWindow.GetCamera()->fitToBbox(rootbox);
            }
            else 
            {
                myWindow.GetCamera()->fitToBbox(sceneState.visibleBounds);
            }
            
        }
        wasPressed = isPressed;

        // Scene Node
        scenePanel.drawHierachy(cadScene, sceneState);
        {
            if (sceneState.selection.has_value()) {
                SceneNode* node = cadScene.resolveSelection(sceneState.selection.value(), runtimeNodes);

                if (node) {
                    ImGui::SetNextWindowPos(ImVec2((float)width, 0.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
                    ImGui::SetNextWindowSize(ImVec2(250.0f, 200.0f), ImGuiCond_Always);
                    ImGui::Begin("Node Info");

                    ImGui::Text("Node name: %s", node->name.c_str());
                    ImGui::Text("Node ID: %zu", node->id);
                    ImGui::Text("Node label: %s", node->occLabelEntry.c_str());
                    ImGui::Text("Color source: %s", node->colorStyle.toSourceString().c_str());
                    ImGui::Text("Node visible: %d", node->isVisible);
                    
                    if (node->prototypeIndex.has_value()) {
                        ImGui::Text("Prototype Name: %s", cadScene.docs[sceneState.selection->documentIndex].prototypes[node->prototypeIndex.value()].name.c_str());
                    }

                    ImGui::End();
                }
            }
        }

        // Info
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, (float)height), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
            ImGui::SetNextWindowSize(ImVec2(400.0f, 150.0f), ImGuiCond_Always);
            ImGui::Begin("Scene Info");
            ImGui::Text("Hello Dear ImGui!");

            for (int i = 0; i<cadScene.docs.size(); i++) {
                const auto& doc = cadScene.docs[i];
                for (const auto& prototype : doc.prototypes) {
                    numPrototype++;
                    if (prototype.meshStatus == MeshStatus::Ready) {
                        numTriangles += prototype.mesh.indices.size() / 3;
                    }
                }
                numSceneNode += (runtimeNodes[i].size() - 1);
            }
            ImGui::Text("Number of document: %zu", cadScene.docs.size());
            ImGui::Text("Number of prototype: %zu", numPrototype);
            ImGui::Text("Number of node: %zu", numSceneNode);
            ImGui::Text("Number of triangle: %zu", numTriangles);

            ImGui::Text("Framebuffer width: %d, height: %d", width, height);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        const bool clickedUiBlank =
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
            !ImGui::IsAnyItemHovered();

        if (clickedUiBlank) {
            sceneState.selection.reset();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(myWindow.GetWindow());
    }

    return 0;
}