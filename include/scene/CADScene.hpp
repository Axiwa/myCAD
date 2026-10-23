#pragma once

#include <vector>
#include <iostream>
#include <stack>

#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_Name.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <filesystem>

#include "common/Bbox.hpp"
#include "geometry/MeshData.hpp"
#include "render/PickingFrameBuffer.hpp"
#include <unordered_map>
#include <unordered_set>

enum class MeshStatus{
    Ready,
    Empty,
    Invalid
};

enum class ColorSource {
    Default,
    DefinitionGeneral,
    DefinitionSurface,
    OccurrenceGeneral,
    OccurrenceSurface
};

enum class UpdateListAction : uint32_t {
    kRemove,
    kUpdate,
    kAdd,
    kAddAndUpdate
};

struct ResolvedStyle {
    glm::vec4 surfaceColor{1.0f};
    ColorSource colorSource = ColorSource::Default;
    std::string toSourceString() {
        switch (colorSource) {
        case ColorSource::DefinitionGeneral:
            return "DefinitionGeneral";
        case ColorSource::DefinitionSurface:
            return "DefinitionSurface";
        case ColorSource::OccurrenceGeneral:
            return "OccurrenceGeneral";
        case ColorSource::OccurrenceSurface:
            return "OccurrenceSurface";
        default:
            return "Default";
        }
    }
};

// definitionlabel
struct PartPrototype {
    std::string labelEntry;
    std::string name;
    TopoDS_Shape definitionShape;
    MeshData mesh;
    MeshStatus meshStatus = MeshStatus::Empty;
    Bbox localBbox;
    std::string toMeshStatusString(){
        switch (meshStatus) {
        case MeshStatus::Ready:
            return "Ready";
        case MeshStatus::Invalid:
            return "Invalid";
        default:
            return "Empty";
        }
    }
};

using SceneNodeId = std::uint64_t;

struct SceneSelection {
    std::size_t documentIndex;
    SceneNodeId nodeId;

    bool operator==(const SceneSelection& other) const {
        return documentIndex == other.documentIndex && nodeId == other.nodeId;
    }
};

struct SceneSelectionHash {
    std::size_t operator()(const SceneSelection& s) const noexcept {
        std::size_t h1 = std::hash<std::size_t>{}(s.documentIndex);
        std::size_t h2 = std::hash<SceneNodeId>{}(s.nodeId);
        return h1 ^ (h2 << 1);
    }
};

// occurrence. Real hierachy
struct SceneNode {
    SceneNodeId id = 0;
    std::string occLabelEntry;

    std::string name;
    glm::dmat4 localTransform{1.0};
    glm::dmat4 worldTransform{1.0};
    std::optional<std::size_t> prototypeIndex; // if this isn't a leaf node, there is no prototypeIndex
    ResolvedStyle colorStyle;

    SceneNode* parent = nullptr;
    std::vector<SceneNode*> children;

    bool isVisible = true;
    bool effectiveVisible = true;
    bool isTransformDirty = true;
    bool isPendingKill = false;

    Bbox worldBbox;

    size_t documentIndex = 0;

    void addChild(SceneNode* child){
        auto it = find(children.begin(), children.end(), child);
        if (it == children.end()){
            children.push_back(child);
        }
    }

    void removeChild(SceneNode* child){
        auto it = find(children.begin(), children.end(), child);
        if (it != children.end()){
            children.erase(it);
        }
    }
};

class SceneNodeUpdateList {
public:
    void addNode(SceneNode* item)
    {
        auto it = nodeActions.find(item);
        if (it != nodeActions.end())
            (uint32_t&)it->second |= (uint32_t)UpdateListAction::kAdd;
        else
            nodeActions[item] = UpdateListAction::kAdd;
    }
    void updateNode(SceneNode* item)
    {
        auto it = nodeActions.find(item);
        if (it != nodeActions.end())
            (uint32_t&)it->second |= (uint32_t)UpdateListAction::kUpdate;
        else
            nodeActions[item] = UpdateListAction::kUpdate;
    }
    void removeNode(SceneNode* item)
    {
        nodeActions[item] = UpdateListAction::kRemove;
    }
    auto begin()
    {
        return nodeActions.begin();
    }
    auto end()
    {
        return nodeActions.end();
    }
    std::unordered_map<SceneNode*, UpdateListAction>::iterator erase(std::unordered_map<SceneNode*, UpdateListAction>::iterator it)
    {
        return nodeActions.erase(it);
    }
    void clear()
    {
        nodeActions.clear();
    }

    std::unordered_map<SceneNode*, UpdateListAction> nodeActions;
};

// list of scenenode
struct CadDocument {
    CadDocument(uint32_t id): docId(id) {}
    CadDocument(const CadDocument&) = delete;
    CadDocument(CadDocument&&) = default;
    CadDocument& operator=(const CadDocument&) = delete;
    std::filesystem::path sourcePath;
    occ::handle<TDocStd_Document> m_document;

    std::vector<PartPrototype> prototypes;

    std::vector<SceneNode*> roots;
    std::vector<std::unique_ptr<SceneNode>> docNodes;
    
    std::unordered_map<SceneNodeId, SceneNode*> nodeLookup;
    std::unordered_set<SceneNodeId> toKill;

    uint64_t numNodes = 0;
    uint32_t docId = 0;
    SceneNodeId nextNodeId = 1;

    SceneNode* createNode(){
        std::unique_ptr<SceneNode> node(new SceneNode());
        node->id = nextNodeId++;

        SceneNode* nodeptr = node.get();
        docNodes.push_back(std::move(node));
        nodeLookup.emplace(nodeptr->id, nodeptr);

        numNodes++;
        return nodeptr;
    }

    void setParent(SceneNode* node, SceneNode* parent){
        node->parent = parent;
        if (parent){
            parent->addChild(node);
        }
    }

    void destroyNodeDry(SceneNode* node, SceneNodeUpdateList& nodeUpdateList){
        for(auto& child: node->children){
            destroyNodeDry(child, nodeUpdateList);
        }
        nodeUpdateList.removeNode(node);
        node->isPendingKill = true;
        toKill.emplace(node->id);
    }

    void destroyNode(SceneNode* node){
        if (!node) return;
        // clean nodelookup
        nodeLookup.erase(node->id);

        // clean root
        roots.erase(std::remove(roots.begin(), roots.end(), node), roots.end());

        // clean parent's child
        if (node->parent){
            node->parent->removeChild(node);
        }

        // clean docNodes
        auto it = std::find_if(docNodes.begin(), docNodes.end(), [node](const std::unique_ptr<SceneNode>& p) { return p.get() == node; });
        if (it != docNodes.end()){
            docNodes.erase(it);
        }

        numNodes--;
    }

    void flush(){
        if (toKill.empty()) return;
        auto pendingKill = [](const SceneNode* node){
            return node->isPendingKill;
        };

        for(auto id: toKill){
            nodeLookup.erase(id);
        }
        roots.erase(std::remove_if(roots.begin(), roots.end(), pendingKill), roots.end());

        for (const auto& node : docNodes) {
            if (node->isPendingKill) continue;

            auto& children = node->children;
            children.erase(std::remove_if(children.begin(), children.end(), pendingKill), children.end());
        }
        docNodes.erase(std::remove_if(docNodes.begin(), docNodes.end(), [](const auto& node){ return node->isPendingKill; }), docNodes.end());

        numNodes = docNodes.size();
        toKill.clear();
    }

    void clear() {
        sourcePath.clear();
        m_document.Nullify();
        prototypes.clear();
        for(int i = 0; i<roots.size(); i++){
            roots[i] = nullptr;
        }
        roots.clear();
        docNodes.clear();
        nodeLookup.clear();
        toKill.clear();
        numNodes = 0;
    }
};

struct CadScene {
    std::vector<CadDocument> docs;
    std::unordered_map<size_t, CadDocument*> docLookup; // for later use

    SceneNode* resolveSelection(SceneSelection& selection){
        if (selection.documentIndex >= docs.size()) {
            return nullptr;
        }

        auto& doc = docs[selection.documentIndex];
        if (doc.nodeLookup.find(selection.nodeId) == doc.nodeLookup.end()) {
            return nullptr;
        }
        
        SceneNode* node = doc.nodeLookup[selection.nodeId];
        if (!node || node->id != selection.nodeId) {
            return nullptr;
        }

        return node;
    }
};

struct SceneRuntimeState {
    std::optional<SceneSelection> selection;
    std::optional<size_t> docSelection;
    Bbox visibleBounds;
    bool visibleBoundsDirty = true;
    bool sceneStructureDirty = true;
};