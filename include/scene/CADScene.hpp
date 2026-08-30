#pragma once

#include <vector>
#include <iostream>

#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_Name.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <filesystem>

#include "common/Bbox.hpp"
#include "geometry/MeshData.hpp"

// definitionlabel
struct PartPrototype {
    std::string labelEntry;
    std::string name;
    TopoDS_Shape definitionShape;
    MeshData mesh;
    Bbox localBbox;
    glm::dmat4 localTransform{1.0};
};

// occurrence. Real hierachy
struct SceneNode {
    std::string name;
    glm::dmat4 localTransform{1.0};
    glm::dmat4 worldTransform{1.0};
    glm::vec4 color{ 1,1,1,1 };
    std::optional<std::size_t> prototypeIndex; // if this isn't a leaf node, there is no prototypeIndex
    std::vector<SceneNode> children;
};

// list of scenenode
struct CadDocument {
    std::filesystem::path sourcePath;
    occ::handle<TDocStd_Document> m_document;

    std::vector<PartPrototype> prototypes;
    std::vector<SceneNode> roots;
};

struct CadScene {
    std::vector<CadDocument> docs;
};