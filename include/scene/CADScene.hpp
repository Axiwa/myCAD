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

// occurrence. Real hierachy
struct SceneNode {
    std::string name;
    glm::dmat4 localTransform{1.0};
    glm::dmat4 worldTransform{1.0};
    std::optional<std::size_t> prototypeIndex; // if this isn't a leaf node, there is no prototypeIndex
    ResolvedStyle colorStyle;
    std::vector<SceneNode> children;
};

// list of scenenode
struct CadDocument {
    std::filesystem::path sourcePath;
    occ::handle<TDocStd_Document> m_document;

    std::vector<PartPrototype> prototypes;
    std::vector<SceneNode> roots;

    void clear() {
        sourcePath.clear();
        m_document.Nullify();
        prototypes.clear();
        roots.clear();
    }
};

struct CadScene {
    std::vector<CadDocument> docs;
};