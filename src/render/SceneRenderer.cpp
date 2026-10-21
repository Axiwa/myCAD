#include <filesystem>
#include <optional>
#include <unordered_map>
#include <set>
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

void SceneRenderer::renderScene(OrbitCamera* const myCamera, CadScene& cadScene, SceneRuntimeState& state, RenderStats& renderStats, SceneNodeUpdateList& updateList, const DraftAnalysisSettings& draftSetting){
    std::optional<RenderItem> selected;
    if (state.sceneStructureDirty || state.visibleBoundsDirty){
        renderItems.clear();

        for(int i = 0; i < cadScene.docs.size(); i++){
            const auto& doc = cadScene.docs[i];
            for (int j = 0; j < doc.docNodes.size(); j++) {
                auto curr = doc.docNodes[j].get();
                if (!curr->isPendingKill && curr->effectiveVisible && curr->prototypeIndex.has_value()){
                    auto index = curr->prototypeIndex.value();

                    if (index < meshBuffers[i].size()){
                        auto& mesh = doc.prototypes[index].mesh;
                        if (meshBuffers[i][index].has_value()){
                            glm::mat4 model = curr->worldTransform;
                            const bool mirrored = glm::determinant(glm::dmat3(model)) < 0.0;
                            
                            // update scene bbox
                            if (state.sceneStructureDirty || state.visibleBoundsDirty) {
                                Bbox nodebox = curr->worldBbox; // bbox has been updated
                                state.visibleBounds.expand(nodebox.minimum);
                                state.visibleBounds.expand(nodebox.maximum);

                                InstanceKey key {(size_t)i, index, curr->colorStyle.surfaceColor, mirrored};
                                RenderItem item {key, curr->worldTransform, nodebox};
                                renderItems.emplace_back(item);
                            }
                        }
                    }
                }
            }
        }
        std::sort(renderItems.begin(), renderItems.end(), [](const RenderItem& a, const RenderItem& b){
            return a.key < b.key;
        });
    }
    renderStats.visibleOccurrences = renderItems.size();

    bool isCullingChanged = (enableCulling && myCamera->calculateFrustum()) || (!wasCulling && enableCulling) || (wasCulling && !enableCulling);

    if (enableInstance){
        instanceProgram.use();
        instanceProgram.setMat4("view", myCamera->viewMatrix());
        instanceProgram.setMat4("proj", myCamera->projectionMatrix());
        instanceProgram.setInt("analyzeDraft", draftSetting.enabled ? 1 : 0);
        if (draftSetting.enabled){
            instanceProgram.setVec3("pullDir", glm::vec3(draftSetting.pullDirection[0], draftSetting.pullDirection[1], draftSetting.pullDirection[2]));
            instanceProgram.setFloat("draftThreshold", draftSetting.requiredAngleDegrees);
        }

        if (!wasInstancing || isCullingChanged) { // no instancing before or culling has changed
            renderBatches.clear();

            int left = 0, right = 0;
            while (left < renderItems.size()) {
                std::vector<glm::mat4> mats;
                while (right < renderItems.size() - 1 && renderItems[right].key == renderItems[right + 1].key) {
                    auto& item = renderItems[right];
                    if (!enableCulling || myCamera->inFrustum(item.nodebbox)){
                        mats.push_back(renderItems[right].worldTransform);
                    }
                    right++;
                }
                if (!enableCulling || myCamera->inFrustum(renderItems[right].nodebbox)){
                    mats.push_back(renderItems[right].worldTransform);
                }
                else{
                    renderStats.culledOccurrences++;
                }

                RenderBatch batch;
                batch.key = renderItems[right].key;
                batch.instanceCount = static_cast<GLsizei>(mats.size());
                batch.uploadData(mats);

                renderBatches.emplace(batch.key, std::move(batch));

                left = right + 1;
                right = left;
            }
        }
        else {
            std::set<InstanceKey> diffKeys;
            for(auto it = updateList.begin(); it != updateList.end(); it++){
                // It can only handle remove and add
                UpdateListAction action = it->second;
                SceneNode* curr = it->first;
                if (!curr) continue;

                size_t documentIndex = curr->documentIndex;
                SceneNodeId nodeId = curr->id;
                
                if (!curr->prototypeIndex.has_value()) continue;

                glm::mat4 model = curr->worldTransform;
                const bool mirrored = glm::determinant(glm::dmat3(model)) < 0.0;

                InstanceKey key {documentIndex, curr->prototypeIndex.value(), curr->colorStyle.surfaceColor, mirrored};
                diffKeys.insert(key);
            }

            for(auto& key: diffKeys) {
                int left = 0;
                while(left < renderItems.size()){
                    if (renderItems[left].key == key){
                        break;
                    }
                    left++;
                }

                if (left < renderItems.size()){
                    size_t right = left;

                    std::vector<glm::mat4> mats;
                    while (right < renderItems.size() - 1 && renderItems[right].key == renderItems[right + 1].key) {
                        auto& item = renderItems[right];
                        if (!enableCulling || myCamera->inFrustum(item.nodebbox)){
                            mats.push_back(renderItems[right].worldTransform);
                        }
                        right++;
                    }
                    if (!enableCulling || myCamera->inFrustum(renderItems[right].nodebbox)){
                        mats.push_back(renderItems[right].worldTransform);
                    }

                    auto [it, inserted] = renderBatches.try_emplace(key);
                    auto& batch = it->second;

                    batch.key = renderItems[right].key;
                    batch.instanceCount = static_cast<GLsizei>(mats.size());
                    batch.uploadData(mats);
                }
                else{
                    auto it = renderBatches.find(key);
                    if (it != renderBatches.end())
                        it->second.instanceCount = 0;
                }
            }
        }
        
        renderStats.culledOccurrences = renderItems.size();
        for (auto& batchpair : renderBatches) {
            auto& batch = batchpair.second;
            if (batch.instanceCount > 0) {
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

                renderStats.culledOccurrences-=batch.instanceCount;
            }
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

        for(auto& item: renderItems){
            if (enableCulling && !myCamera->inFrustum(item.nodebbox)){
                renderStats.culledOccurrences++;
                continue;
            }
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

    if (state.selection.has_value()){       
        size_t documentIndex = state.selection->documentIndex;
        SceneNodeId nodeId = state.selection->nodeId;

        SceneNode* curr = nullptr;
        if (cadScene.docs[documentIndex].nodeLookup.find(nodeId) != cadScene.docs[documentIndex].nodeLookup.end()){
            curr = cadScene.docs[documentIndex].nodeLookup[nodeId];
        }

        if (curr && !curr->isPendingKill && curr->effectiveVisible && (!enableCulling || myCamera->inFrustum(curr->worldBbox)) && curr->prototypeIndex.has_value()){
            size_t prototypeIndex = curr->prototypeIndex.value();

            shaderProgram.use();
            shaderProgram.setMat4("view", myCamera->viewMatrix());
            shaderProgram.setMat4("proj", myCamera->projectionMatrix());
            shaderProgram.setInt("analyzeDraft", 0);

            glm::mat4 model = curr->worldTransform;
            const bool mirrored = glm::determinant(glm::dmat3(model)) < 0.0;
            glm::vec4 col = glm::vec4(1, 1, 0, 1);
            glFrontFace(mirrored ? GL_CW : GL_CCW);

            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
            shaderProgram.setMat4("model", model);
            shaderProgram.setVec4("baseColor", col);
            meshBuffers[documentIndex][prototypeIndex].value().draw();
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);

            auto& mesh = cadScene.docs[documentIndex].prototypes[prototypeIndex].mesh;
            renderStats.drawCalls++;
            renderStats.submittedTriangles += mesh.indices.size() / 3;
        }
    }

    wasInstancing = enableInstance;
    wasCulling = enableCulling;
    updateList.clear();
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
                if (curr->isVisible == false || curr->isPendingKill) {
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