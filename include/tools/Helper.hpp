#pragma once

#include "scene/CADScene.hpp"
#include "glm/glm.hpp"

void StressTest(bool isStressTest, CadScene& cadScene, SceneNodeUpdateList& nodeUpdateList, SceneRuntimeState& sceneState) {
    if (!sceneState.selection.has_value()) return;

    SceneNode* node = cadScene.resolveSelection(sceneState.selection.value());
    if (!node || !node->prototypeIndex.has_value()) return;

    size_t prototypeIndex = node->prototypeIndex.value();
    size_t docIndex = node->documentIndex;

    auto& doc = cadScene.docs[docIndex];
    const auto prototype = doc.prototypes[prototypeIndex];
    if (isStressTest){ // to add node with prototype 0
        const glm::dmat4 baseTransform = node->worldTransform;
        auto colorStyle = ResolvedStyle();
        colorStyle.surfaceColor = glm::vec4(0.2f, 0.3f, 0.5f, 1.0f);
        const auto baseName = prototype.name;
        const auto baseEntry = prototype.labelEntry;

        uint64_t oriNum = doc.numNodes + 1;
        uint64_t num = 256;

        const double halfExtent = (static_cast<double>(num) - 1.0) * 0.5;
        SceneNode* newroot = doc.createNode();
        newroot->name = "stress root";
        nodeUpdateList.addNode(newroot);
        doc.roots.push_back(newroot);
        for (uint64_t k = 0; k < num * num; k++) {
            const SceneNodeId i = oriNum + k;
            const double col = static_cast<double>(k / num) - halfExtent;
            const double row = static_cast<double>(k % num) - halfExtent;
            const glm::dmat4 offset = glm::translate(glm::dmat4(1), glm::dvec3(col, row, 0.0));
            SceneNode* newnode = doc.createNode();
            newnode->parent = newroot;
            newnode->occLabelEntry = baseEntry + "/" + std::to_string(i);
            newnode->name = baseName + "/" + std::to_string(i);
            newnode->localTransform = offset * baseTransform;
            newnode->worldTransform = newnode->localTransform;
            newnode->colorStyle = colorStyle;
            if (i % 2 == 0){
                newnode->worldTransform[0] = -newnode->worldTransform[0];
                newnode->colorStyle.surfaceColor = glm::vec4(0.5f, 0.2f, 0.3f, 1.0f);
            }
            newnode->prototypeIndex = prototypeIndex;
            
            Bbox nodebox = prototype.localBbox;
            nodebox.applyTransform(newnode->worldTransform);
            newnode->worldBbox = nodebox;
            newnode->isTransformDirty = false;
            newroot->children.push_back(newnode);
            nodeUpdateList.addNode(newnode);
        }
    }
    else{ // to remove node
        SceneNode* newroot = nullptr;
        for(auto& node: doc.docNodes){
            if (node->name == "stress root"){
                newroot = node.get();
                break;
            }
        }
        doc.destroyNodeDry(newroot, nodeUpdateList);
    }
}