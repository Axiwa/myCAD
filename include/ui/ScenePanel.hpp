#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "IconsFontAwesome6.h"
#include <string>
#include <filesystem>
#include <stack>
#include <cstdint>
#include <unordered_map>

#include "scene/CADScene.hpp"


class ScenePanel{
public:
    ScenePanel(GLFWwindow* window, const std::filesystem::path& executableDirectory){
        const char* glsl_version = "#version 330 core";
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        std::string fontfile = executableDirectory.string() + "/fonts/Roboto-Regular.ttf";
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

        std::string awesomefile = executableDirectory.string() + "/fonts/fa-solid-900.ttf";
        io.Fonts->AddFontFromFileTTF(
            awesomefile.c_str(),
            16.0f,
            &config,
            icon_ranges
        );
    }

    void newFrame(){
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void drawHierarchy(CadScene& cadScene, SceneRuntimeState& state, SceneNode** changingNode){
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(200.0f, 300.f), ImGuiCond_Once);
        ImGui::Begin("Scene Hierachy");
        for (int i = 0; i < cadScene.docs.size(); i++) {
        auto& doc = cadScene.docs[i];
            ImGui::PushID(i);
            if (ImGui::TreeNode(doc.sourcePath.filename().string().c_str())) {
                for (auto& root : doc.roots) {
                    std::stack<std::pair<SceneNode*, bool>> m_stack;
                    m_stack.emplace(root, false);

                    while (!m_stack.empty()) {
                        SceneNode* curr = m_stack.top().first;
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

                        if (state.selection.has_value() && state.selection->documentIndex == i && state.selection->nodeId == curr->id) {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        // 眼睛按钮
                        if (ImGui::SmallButton(curr->isVisible ? ICON_FA_EYE "##visibility" : ICON_FA_EYE_SLASH "##visibility"))
                        {
                            *changingNode = curr;
                            curr->isVisible = !curr->isVisible;
                            curr->effectiveVisible = curr->parent ? curr->parent->effectiveVisible && curr->isVisible : curr->isVisible;
                            state.visibleBoundsDirty = true;
                        }

                        ImGui::SameLine();
                        const bool open = ImGui::TreeNodeEx(std::to_string(curr->id).c_str(), flags, "%s", curr->name.c_str());
                        
                        if (ImGui::IsItemClicked()) {
                            state.selection.emplace();
                            state.selection->documentIndex = i;
                            state.selection->nodeId = curr->id;
                        }
                        
                        if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
                            m_stack.emplace(curr, true);

                            for (auto it = curr->children.rbegin(); it != curr->children.rend(); it++) {
                                m_stack.emplace(*it, false);
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
    }

    ~ScenePanel(){
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};