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
#include "profiler/profiler.hpp"
#include "geometry/Design.hpp"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

bool isFaceMirror = false;
bool isNodeMirror = false;
bool isStreeTest = true;
bool enableInstance = true;
bool enableCulling = false;

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
    std::filesystem::path exePath = executableDirectory();

    bool needsInitialFit = true;

    CadDocument test;
    std::filesystem::path defaultModel = exePath / "data" / "step" / "linkrods.step";
    std::string path = defaultModel.string();
    if (argc > 1) {
        path = argv[1];
        std::cout << "Import path: " << path << std::endl;
    }
    else{
        std::cout<<"Null import path, use default: " << path <<std::endl;
    }

    if (!xcafImporter.load(path.c_str(), test)) {
        exit(1);
    }

    if (isStreeTest){
        // find first leaf node
        SceneNode* leaf = nullptr;
        for (auto root : test.roots) {
            std::stack<SceneNode*> m_stack;
            m_stack.push(root);
            while (!m_stack.empty()) {
                SceneNode* curr = m_stack.top();
                m_stack.pop();
                if (curr->prototypeIndex.has_value()) {
                    leaf = curr;
                    break;
                }
                for (auto& child : curr->children) {
                    m_stack.push(child);
                }
            }
            if (leaf) break;
        }
        if (!leaf) {
            std::cerr << "No renderable leaf for stress test\n";
            return 1;
        }
        const auto prototypeIndex = leaf->prototypeIndex;
        const auto baseTransform = leaf->worldTransform;
        const auto colorStyle = leaf->colorStyle;
        const auto baseName = leaf->name;
        const auto baseEntry = leaf->occLabelEntry;

        uint64_t oriNum = test.numNodes + 1;
        uint64_t num = 256;
        for (SceneNodeId i = oriNum; i < oriNum + num*num; i++) {
            const glm::dmat4 offset = glm::translate(glm::dmat4(1.0), glm::dvec3((i/num) * 1, (i % num) * 1, 0.0));
            test.roots.push_back(new SceneNode());
            test.roots.back()->id = i;
            test.roots.back()->occLabelEntry = baseEntry + "/" + std::to_string(i);
            test.roots.back()->name = baseName + "/" + std::to_string(i);
            test.roots.back()->localTransform = offset * baseTransform;
            test.roots.back()->worldTransform = test.roots.back()->localTransform;
            if (i % 2 == 0){
                test.roots.back()->worldTransform[0] = -test.roots.back()->worldTransform[0];
            }
            test.roots.back()->prototypeIndex = prototypeIndex;
            test.roots.back()->colorStyle = colorStyle;
            test.numNodes++;
        }
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
    ScenePanel scenePanel(myWindow.GetWindow(), exePath);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    DraftAnalysisSettings draftSetting;

    // Create mesh buffers for cadScene
    SceneRenderer sceneRenderer(exePath);
    sceneRenderer.collectObjects(cadScene);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    bool isPressed = false;
    bool wasPressed = false;

    SceneRuntimeState sceneState;
    Profiler cpuProfiler;
    RenderStats renderStats;
    SceneNodeUpdateList nodeUpdateList;
    uint32_t frameCount = 0;

    size_t numPrototype = 0;
    size_t numSceneNode = 0;

    cadScene.getRuntimeNodes(runtimeNodes);
    for (int i = 0; i<cadScene.docs.size(); i++) {
        const auto& doc = cadScene.docs[i];
        for (const auto& prototype : doc.prototypes) {
            numPrototype++;
            if (prototype.meshStatus == MeshStatus::Ready) {
                renderStats.uniquePrototypeTriangles += prototype.mesh.indices.size() / 3;
            }
        }
        numSceneNode += (runtimeNodes[i].size() - 1);
    }

    std::vector<SceneSelection> pickRegistry;
    std::unordered_map<SceneSelection, PickId, SceneSelectionHash> pickMap;
    glfwSwapInterval(0);
    while(!glfwWindowShouldClose(myWindow.GetWindow())){
        glfwPollEvents();

        renderStats.drawCalls = 0;
        renderStats.visibleOccurrences = 0;
        renderStats.submittedTriangles = 0;
        renderStats.culledOccurrences = 0;
        
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(myWindow.GetWindow(), &width, &height);

        if (width <= 0 || height <= 0) {
            glfwWaitEvents();
            continue;
        }

        // Start the Dear ImGui frame
        scenePanel.newFrame();
        PickId pickedID = 0;

        bool allowViewportMouse = !ImGui::GetIO().WantCaptureMouse;
        myWindow.update(allowViewportMouse);

        // this will not happen for now... randomly adding new variables
        if (sceneState.sceneStructureDirty){
            for(int i = 0; i<runtimeNodes.size(); i++){
                auto& doc = runtimeNodes[i];
                for(auto node: doc){
                    nodeUpdateList.removeNode(node);
                }
            }
            cadScene.getRuntimeNodes(runtimeNodes);
            sceneRenderer.collectObjects(cadScene);

            pickRegistry.clear();
            pickMap.clear();
            for(int i = 0; i<runtimeNodes.size(); i++){
                auto& doc = runtimeNodes[i];
                for(auto node: doc){
                    nodeUpdateList.addNode(node);
                    if (node && node->prototypeIndex.has_value()){
                        auto index = node->prototypeIndex.value();
                        if (cadScene.docs[i].prototypes[index].meshStatus == MeshStatus::Ready){
                            pickRegistry.emplace_back();
                            pickRegistry.back().documentIndex = i;
                            pickRegistry.back().nodeId = node->id;
                            pickMap[pickRegistry.back()] = static_cast<PickId>(pickRegistry.size());
                        }
                    }
                }
            }

            numPrototype = 0;
            numSceneNode = 0;
            renderStats.uniquePrototypeTriangles = 0;

            for (int i = 0; i<cadScene.docs.size(); i++) {
                const auto& doc = cadScene.docs[i];
                for (const auto& prototype : doc.prototypes) {
                    numPrototype++;
                    if (prototype.meshStatus == MeshStatus::Ready) {
                        renderStats.uniquePrototypeTriangles += prototype.mesh.indices.size() / 3;
                    }
                }
                numSceneNode += (runtimeNodes[i].size() - 1);
            }

            sceneState.visibleBoundsDirty = true;
        }

        std::optional<mouseCoord> pixelCoord;
        bool leftPress = myWindow.consumeLeftPress(pixelCoord);
        if (allowViewportMouse && leftPress && pixelCoord.has_value()){
            if (sceneRenderer.offscreenPicking(myWindow.GetCamera(), myWindow.GetPickFramebuffer(), myWindow.GetWindow(), cadScene, pickMap)){
                pickedID = myWindow.GetPickFramebuffer()->pickID(pixelCoord->x, pixelCoord->y);
                std::cout << "Clicked ID: " << pickedID << std::endl;

                if (pickedID > 0 && pickedID <= pickRegistry.size()) {
                    sceneState.selection = pickRegistry[pickedID - 1];
                }
                else {
                    if (pickedID > pickRegistry.size()) {
                        std::cout << "Pick something not pickable!" << std::endl;
                    }
                    pickedID = 0;
                    sceneState.selection.reset();
                }
            }
        }

        if (sceneState.visibleBoundsDirty) {
            sceneState.visibleBounds.reset();
        }

        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (frameCount >= cpuProfiler.warmupCount){
            cpuProfiler.start();
        }
        sceneRenderer.renderScene(myWindow.GetCamera(), runtimeNodes, cadScene, sceneState, renderStats, nodeUpdateList, draftSetting);
        if (frameCount >= cpuProfiler.warmupCount){
            cpuProfiler.accumulate();
        }

        if (needsInitialFit && sceneState.visibleBounds.valid()){
            myWindow.GetCamera()->fitToBbox(sceneState.visibleBounds);
            needsInitialFit = false;
        }
        sceneState.visibleBoundsDirty = false;
        sceneState.sceneStructureDirty = false;

        isPressed = (glfwGetKey(myWindow.GetWindow(), GLFW_KEY_F) == GLFW_PRESS);
        if (isPressed && !wasPressed) {
            if (sceneState.selection.has_value()){
                Bbox rootbox;
                size_t docindex = sceneState.selection->documentIndex;
                SceneNodeId nodeid = sceneState.selection->nodeId;
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
                            size_t prototypeid = curr->prototypeIndex.value();
                            if (prototypeid >= cadScene.docs[docindex].prototypes.size()){
                                continue;
                            }
                            const auto& prototype = cadScene.docs[docindex].prototypes[prototypeid];
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
                            m_stack.push(*it);
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

        if (cpuProfiler.getTotalFrames() >= cpuProfiler.maxCount && cpuProfiler.getTotalFrames() % cpuProfiler.maxCount == 0){
            renderStats.cpuRenderMilliseconds = cpuProfiler.getAvgTime();
        }
        // Scene Node
        SceneNode* changingNode = nullptr;
        scenePanel.drawHierachy(cadScene, sceneState, &changingNode);
        if (changingNode){
            if (changingNode->effectiveVisible == true){
                nodeUpdateList.addNode(changingNode);
            }
            else if (changingNode->effectiveVisible == false) {
                nodeUpdateList.removeNode(changingNode);
            }
            
            std::stack<SceneNode*> m_stack;
            m_stack.push(changingNode);
            while(!m_stack.empty()){
                SceneNode* curr = m_stack.top();
                m_stack.pop();

                for(auto child: curr->children){
                    bool oldVisibility = child->effectiveVisible;
                    child->effectiveVisible = child->isVisible && curr->effectiveVisible;
                    if (oldVisibility == true && child->effectiveVisible == false){
                        nodeUpdateList.removeNode(child);
                    }
                    else if (oldVisibility == false && child->effectiveVisible == true){
                        nodeUpdateList.addNode(child);
                    }

                    m_stack.push(child);
                }
            }
        }

        {
            if (sceneState.selection.has_value()) {
                SceneNode* node = cadScene.resolveSelection(sceneState.selection.value(), runtimeNodes);

                if (node) {
                    ImGui::SetNextWindowPos(ImVec2((float)width, 0.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
                    ImGui::SetNextWindowSize(ImVec2(250.0f, 200.0f), ImGuiCond_Once);
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
            ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f), ImGuiCond_Once);
            ImGui::Begin("Scene Info");
            ImGui::Text("Hello Dear ImGui!");
            ImGui::Checkbox("Instancing", &enableInstance);
            bool cullingChange = ImGui::Checkbox("Culling", &enableCulling);
            if (cullingChange) {
                myWindow.GetCamera()->setDirty();
            }
            ImGui::Text("Number of document: %zu", cadScene.docs.size());
            ImGui::Text("Number of prototype: %zu", numPrototype);
            ImGui::Text("Number of node: %zu", numSceneNode);
            ImGui::Text("Number of unique triangle: %zu", renderStats.uniquePrototypeTriangles);
            ImGui::Text("Number of submitted triangles: %zu", renderStats.submittedTriangles);
            ImGui::Text("Number of visible occurrences: %zu", renderStats.visibleOccurrences);
            ImGui::Text("Number of culled occurrences: %zu", renderStats.culledOccurrences);
            ImGui::Text("Number of draw calls: %zu", renderStats.drawCalls);

            ImGui::Text("Framebuffer width: %d, height: %d", width, height);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Text("Scene render CPU average: %.3f ms", renderStats.cpuRenderMilliseconds);
            ImGui::End();
        }
        {
            ImGui::SetNextWindowPos(ImVec2((float)width, (float)height), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
            ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f), ImGuiCond_Once);
            ImGui::Begin("Analysis");
            ImGui::Checkbox("DraftAngle Analysis", &draftSetting.enabled);
            if (draftSetting.enabled){
                ImGui::Text("Pull Direction");
                if (ImGui::Button("+X")) { draftSetting.pullDirection[0] = 1.0f; draftSetting.pullDirection[1] = 0.0f; draftSetting.pullDirection[2] = 0.0f; } ImGui::SameLine();
                if (ImGui::Button("-X")) { draftSetting.pullDirection[0] = -1.0f; draftSetting.pullDirection[1] = 0.0f; draftSetting.pullDirection[2] = 0.0f; } ImGui::SameLine();
                if (ImGui::Button("+Y")) { draftSetting.pullDirection[0] = 0.0f; draftSetting.pullDirection[1] = 1.0f; draftSetting.pullDirection[2] = 0.0f; } ImGui::SameLine();
                if (ImGui::Button("-Y")) { draftSetting.pullDirection[0] = 0.0f; draftSetting.pullDirection[1] = -1.0f; draftSetting.pullDirection[2] = 0.0f; } ImGui::SameLine();
                if (ImGui::Button("+Z")) { draftSetting.pullDirection[0] = 0.0f; draftSetting.pullDirection[1] = 0.0f; draftSetting.pullDirection[2] = 1.0f; } ImGui::SameLine();
                if (ImGui::Button("-Z")) { draftSetting.pullDirection[0] = 0.0f; draftSetting.pullDirection[1] = 0.0f; draftSetting.pullDirection[2] = -1.0f; }

                ImGui::SliderFloat("Angle Threshold", &draftSetting.requiredAngleDegrees, 0.0f, 30.f, "%.1f");
            }

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
        frameCount++;
    }

    return 0;
}