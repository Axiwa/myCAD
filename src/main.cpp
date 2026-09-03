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
#include "asset/StepXcafImporter.hpp"

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

    // IMGUI
    const char* glsl_version = nullptr;
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controlsv
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(myWindow.GetWindow(), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ShaderProgram shaderProgram;
    std::string vertpath = executableDirectory().string() + "/shader/plain.vert";
    std::string fragpath = executableDirectory().string() + "/shader/plain.frag";
    if (!shaderProgram.LoadShader(vertpath, fragpath)){
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

    while(!glfwWindowShouldClose(myWindow.GetWindow())){
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        int numPrototype = 0;
        int numSceneNode = 0;
        int numTriangles = 0;

        if (!ImGui::GetIO().WantCaptureMouse) {
            if (!myWindow.update()) {
                glfwWaitEvents();
                continue;
            }
        }

        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shaderProgram.use();
        shaderProgram.setMat4("view", myWindow.GetCamera()->viewMatrix());
        shaderProgram.setMat4("proj", myWindow.GetCamera()->projectionMatrix());

        if (world_bbox.outDated){
            world_bbox.reset();
        }

        // Draw cadScene
        for (int i = 0; i < cadScene.docs.size(); i++) {
            const auto& doc = cadScene.docs[i];
            for (const auto& root : doc.roots) {
                std::stack<const SceneNode*> m_stack;
                m_stack.push(&root);
                                
                while (!m_stack.empty()) {
                    const SceneNode* curr = m_stack.top();
                    m_stack.pop();
                    numSceneNode++;

                    if (curr->prototypeIndex.has_value()) {
                        auto index = curr->prototypeIndex.value();

                        if (index < meshBuffers[i].size()){
                            auto& mesh = doc.prototypes[index].mesh;
                            if (meshBuffers[i][index].has_value()){
                                glm::mat4 model = curr->worldTransform;
                                shaderProgram.setMat4("model", model);
                                shaderProgram.setVec4("baseColor", curr->colorStyle.surfaceColor);

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


        // IMGUI
        {
            ImGui::SetNextWindowSize(ImVec2(400.0f, 150.0f), ImGuiCond_Always);
            ImGui::Begin("Scene Info");
            ImGui::Text("Hello Dear ImGui!");

            for (const auto& doc : cadScene.docs) {
                for (const auto& prototype : doc.prototypes) {
                    numPrototype++;
                    if (prototype.meshStatus == MeshStatus::Ready) {
                        numTriangles += prototype.mesh.indices.size() / 3;
                    }
                }
            }
            ImGui::Text("Number of document: %d", cadScene.docs.size());
            ImGui::Text("Number of prototype: %d", numPrototype);
            ImGui::Text("Number of node: %d", numSceneNode);
            ImGui::Text("Number of triangle: %d", numTriangles);

            ImGui::Text("Framebuffer width: %f, height: %f", io.DisplaySize.x, io.DisplaySize.y);
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