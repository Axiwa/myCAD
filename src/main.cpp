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
#include "asset/StepXcafImporter.hpp"
#include "IconsFontAwesome6.h"

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
    std::vector<std::vector<SceneNode>> runtimeNodes;

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

    Bbox world_bbox;

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
    const char* glsl_version = "#version 330 core";
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(myWindow.GetWindow(), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::string fontfile = executableDirectory().string() + "/fonts/Roboto-Regular.ttf";
    io.Fonts->AddFontFromFileTTF(
        fontfile.c_str(),
        16.0f
    );

    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;

    static const ImWchar icon_ranges[] =
    {
        ICON_MIN_FA,
        ICON_MAX_FA,
        0
    };

    std::string awesomefile = executableDirectory().string() + "/fonts/fa-solid-900.ttf";
    io.Fonts->AddFontFromFileTTF(
        awesomefile.c_str(),
        16.0f,
        &config,
        icon_ranges
    );

    std::string vertpath = executableDirectory().string() + "/shader/plain.vert";
    std::string fragpath = executableDirectory().string() + "/shader/plain.frag";
    std::string pickpath = executableDirectory().string() + "/shader/offscreen.frag";
    std::string highlightpath = executableDirectory().string() + "/shader/highlight.frag";

    ShaderProgram pickFirstProgram;
    if (!pickFirstProgram.LoadShader(vertpath, pickpath)) {
        return 1;
    }

    ShaderProgram shaderProgram;
    if (!shaderProgram.LoadShader(vertpath, fragpath)){
        return 1;
    }

    ShaderProgram pickSecondProgram;
    if (!pickSecondProgram.LoadShader(vertpath, highlightpath)) {
        return 1;
    }

    // Create mesh buffers for cadScene
    std::vector<std::vector<std::optional<GpuMesh>>> meshBuffers(cadScene.docs.size());
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

    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    view = glm::lookAt(glm::vec3(3,3,3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));


    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    bool isPressed = false;
    bool wasPressed = false;
    std::optional<SceneSelection> selection;
    cadScene.getRuntimeNodes(runtimeNodes);

    std::vector<SceneSelection> pickRegistry;
    std::unordered_map<SceneSelection, PickId, SceneSelectionHash> pickMap;
    for(int i = 0; i<runtimeNodes.size(); i++){
        auto& doc = runtimeNodes[i];
        for(auto& node: doc){
            if (node.prototypeIndex.has_value()){
                auto index = node.prototypeIndex.value();
                if (cadScene.docs[i].prototypes[index].meshStatus == MeshStatus::Ready){
                    pickRegistry.emplace_back();
                    pickRegistry.back().documentIndex = i;
                    pickRegistry.back().nodeId = node.id;
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

        if (!myWindow.update(!ImGui::GetIO().WantCaptureMouse)) {
            glfwWaitEvents();
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (world_bbox.outDated) {
            world_bbox.reset();
        }

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

        if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            selection.reset();
        }
        PickId pickedID = myWindow.GetPickFramebuffer()->getID();
        if (pickedID > 0) {
            selection = pickRegistry[pickedID-1];
        }

        // ============================================
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
                                if (selection.has_value() && selection->documentIndex == i && selection->nodeId == curr->id) {
                                    glm::vec4 col = glm::vec4(1, 1, 0, 1);
                                    shaderProgram.setVec4("baseColor", col);
                                }
                                else {
                                    shaderProgram.setVec4("baseColor", curr->colorStyle.surfaceColor);
                                }
                                
                                // update scene bbox
                                if (world_bbox.outDated) {
                                    for (auto& vert : mesh.vertices) {
                                        glm::vec3 pos = model * glm::vec4(vert.position, 1);
                                        world_bbox.expand(pos);
                                    }
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
        if (needsInitialFit && world_bbox.valid()){
            myWindow.GetCamera()->fitToBbox(world_bbox);
            needsInitialFit = false;
        }
        world_bbox.outDated = false;

        isPressed = (glfwGetKey(myWindow.GetWindow(), GLFW_KEY_F) == GLFW_PRESS);
        if (isPressed && !wasPressed) {
            myWindow.GetCamera()->fitToBbox(world_bbox);
        }
        wasPressed = isPressed;

        int width, height;
        glfwGetFramebufferSize(myWindow.GetWindow(), &width, &height);

        // Scene Node
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(200.0f, 300.f), ImGuiCond_Once);
            ImGui::Begin("Scene Hierachy");
            for (int i = 0; i < cadScene.docs.size(); i++) {
                const auto& doc = cadScene.docs[i];
                ImGui::PushID(i);
                if (ImGui::TreeNode(doc.sourcePath.filename().string().c_str())) {
                    for (const auto& root : doc.roots) {
                        std::stack<std::pair<const SceneNode*, bool>> m_stack;
                        m_stack.emplace(&root, false);

                        while (!m_stack.empty()) {
                            const SceneNode* curr = m_stack.top().first;
                            bool touched = m_stack.top().second;
                            m_stack.pop();
                            ImGui::PushID(curr);

                            bool isLeaf = curr->children.empty();
                                    
                            if (touched) {
                                // Noneed to test isLeaf because leaf will not enter again
                                ImGui::TreePop();
                                ImGui::PopID();
                                continue;
                            }

                            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                            if (isLeaf) {
                                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                            }

                            if (selection.has_value() && selection->documentIndex == i && selection->nodeId == curr->id) {
                                flags |= ImGuiTreeNodeFlags_Selected;
                            }

                            // 眼睛按钮
                            if (ImGui::SmallButton(curr->isVisible ? ICON_FA_EYE "##visibility" : ICON_FA_EYE_SLASH "##visibility"))
                            {
                                curr->isVisible = !curr->isVisible;
                                world_bbox.outDated = true;
                            }

                            ImGui::SameLine();
                            const bool open = ImGui::TreeNodeEx(std::to_string(curr->id).c_str(), flags, "%s", curr->name.c_str());
                            
                            if (ImGui::IsItemClicked()) {
                                selection.emplace();
                                selection->documentIndex = i;
                                selection->nodeId = curr->id;
                                if (pickMap.find(selection.value()) != pickMap.end()){
                                    myWindow.GetPickFramebuffer()->setID(pickMap[selection.value()]);
                                }
                                else {
                                    myWindow.GetPickFramebuffer()->setID(0);
                                }
                            }
                            
                            if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
                                m_stack.emplace(curr, true);

                                for (auto it = curr->children.rbegin(); it != curr->children.rend(); it++) {
                                    m_stack.emplace(&(*it), false);
                                }
                            }

                            ImGui::PopID();
                        }
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::End();

            if (selection.has_value()) {
                SceneNode& node = runtimeNodes[selection->documentIndex][selection->nodeId-1];

                ImGui::SetNextWindowPos(ImVec2((float)width, 0.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
                ImGui::SetNextWindowSize(ImVec2(250.0f, 200.0f), ImGuiCond_Always);
                ImGui::Begin("Node Info");

                ImGui::Text("Node name: %s", node.name.c_str());
                ImGui::Text("Node ID: %zu", node.id);
                ImGui::Text("Node label: %s", node.occLabelEntry.c_str());
                ImGui::Text("Color source: %s", node.colorStyle.toSourceString().c_str());
                
                if (node.prototypeIndex.has_value()) {
                    ImGui::Text("Prototype Name: %s", cadScene.docs[selection->documentIndex].prototypes[node.prototypeIndex.value()].name.c_str());
                }

                ImGui::End();
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
                numSceneNode += runtimeNodes[i].size();
            }
            ImGui::Text("Number of document: %zu", cadScene.docs.size());
            ImGui::Text("Number of prototype: %zu", numPrototype);
            ImGui::Text("Number of node: %zu", numSceneNode);
            ImGui::Text("Number of triangle: %zu", numTriangles);

            ImGui::Text("Framebuffer width: %d, height: %d", width, height);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(myWindow.GetWindow());
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}