#include "asset/StepXcafImporter.hpp"
#include "geometry/OcctMesher.hpp"
#include "common/common.h"
#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_Name.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <iostream>
#include <string>
#include <unordered_map>
#include <filesystem>

static void DumpTrsf(const gp_Trsf& trsf) {
    for (int i = 1; i < 4; i++) {
        for (int j = 1; j <= 4; j++) {
            std::cout << trsf.Value(i, j)<<" ";
        }
        std::cout << std::endl;
    }
}

gp_Trsf ToOcctTransform(const glm::dmat4& m) {
    gp_Trsf T;
    gp_Mat rot(
        m[0][0], m[1][0], m[2][0],
        m[0][1], m[1][1], m[2][1],
        m[0][2], m[1][2], m[2][2]
    );

    T.SetValues(
        rot.Value(1, 1), rot.Value(1, 2), rot.Value(1, 3), m[3][0],
        rot.Value(2, 1), rot.Value(2, 2), rot.Value(2, 3), m[3][1],
        rot.Value(3, 1), rot.Value(3, 2), rot.Value(3, 3), m[3][2]
    );

    return T;
}

glm::dmat4 ToGLMTransform(const gp_Trsf& T) {
    glm::dmat4 m(1.0);
    for (int i = 1; i < 4; i++) {
        for (int j = 1; j <= 4; j++) {
            m[j - 1][i - 1] = T.Value(i, j);
        }
    }
    return m;
}


static std::string ToUtf8(const TCollection_ExtendedString& s)
{
    const Standard_Integer len = s.LengthOfCString();
    std::string out(len, '\0');
    auto data = out.data();
    s.ToUTF8CString(data);
    return out;
}


SceneNode traverse(const occ::handle<XCAFDoc_ShapeTool>& shapeTool, const TDF_Label& root, std::unordered_map<std::string, uint32_t>& partsmap, std::vector<PartPrototype>& partsvec, const gp_Trsf& parent, int level) {
    SceneNode node;

    TDF_Label occurrenceLabel = root;
    TDF_Label definitionLabel = root;

    std::cout << "--------------------------------" << std::endl;
    std::cout << "Transform Level: " << level << "\n";
    
    const TopLoc_Location location = XCAFDoc_ShapeTool::GetLocation(occurrenceLabel);
    gp_Trsf local = location.Transformation();
    gp_Trsf world = parent * local;
    node.localTransform = ToGLMTransform(local);
    node.worldTransform = ToGLMTransform(world);

    Quantity_ColorRGBA color;
    if (XCAFDoc_ColorTool::GetColor(occurrenceLabel, XCAFDoc_ColorSurf, color))
        node.color = glm::vec4(color.GetRGB().Red(), color.GetRGB().Green(), color.GetRGB().Blue(), color.Alpha());

    if (XCAFDoc_ColorTool::GetColor(occurrenceLabel, XCAFDoc_ColorGen, color))
        node.color = glm::vec4(color.GetRGB().Red(), color.GetRGB().Green(), color.GetRGB().Blue(), color.Alpha());

    TCollection_AsciiString entry;
    TCollection_AsciiString dentry;

    if (XCAFDoc_ShapeTool::IsReference(occurrenceLabel)) {
        if (!XCAFDoc_ShapeTool::GetReferredShape(occurrenceLabel, definitionLabel)) {
            return node;
        }
        if (definitionLabel.IsNull()) {
            std::cout << "Lable: null" << std::endl;
            return node;
        }

        if (XCAFDoc_ColorTool::GetColor(definitionLabel, XCAFDoc_ColorSurf, color))
            node.color = glm::vec4(color.GetRGB().Red(), color.GetRGB().Green(), color.GetRGB().Blue(), color.Alpha());

        if (XCAFDoc_ColorTool::GetColor(definitionLabel, XCAFDoc_ColorGen, color))
            node.color = glm::vec4(color.GetRGB().Red(), color.GetRGB().Green(), color.GetRGB().Blue(), color.Alpha());
    }

    TDF_Tool::Entry(occurrenceLabel, entry);
    TDF_Tool::Entry(definitionLabel, dentry);
    std::cout << "Occurrence Label entry: " << entry << std::endl;
    std::cout << "Definition Label entry: " << dentry << std::endl;
    std::cout << "Tag: " << definitionLabel.Tag() << "\n";
    std::cout << "Depth: " << definitionLabel.Depth() << "\n";
    std::cout << "NbAttributes: " << definitionLabel.NbAttributes() << "\n";
    std::cout << "IsShape: " << XCAFDoc_ShapeTool::IsShape(definitionLabel) << "\n";
    std::cout << "IsAssembly: " << XCAFDoc_ShapeTool::IsAssembly(definitionLabel) << "\n";
    std::cout << "IsComponent: " << XCAFDoc_ShapeTool::IsComponent(definitionLabel) << "\n";
    std::cout << "IsSimpleShape: " << XCAFDoc_ShapeTool::IsSimpleShape(definitionLabel) << "\n";

    TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(definitionLabel);
    if (!shape.IsNull())
    {
        std::cout << "Shape type: " << (int)shape.ShapeType() << "\n";
    }
    else
    {
        std::cout << "Shape: <none>\n";
    }

    occ::handle<TDataStd_Name> name;
    if (occurrenceLabel.FindAttribute(TDataStd_Name::GetID(), name)) {
        std::cout << "Occurence Name: " << name->Get() << std::endl;
        node.name = ToUtf8(name->Get());
    }
    else if (definitionLabel.FindAttribute(TDataStd_Name::GetID(), name)) {
        std::cout << "Definition Name: " << name->Get() << std::endl;
        node.name = ToUtf8(name->Get());
    }
    else{
        node.name = "default";
    }

    if (XCAFDoc_ShapeTool::IsAssembly(definitionLabel)) {
        NCollection_Sequence<TDF_Label> components;
        shapeTool->GetComponents(definitionLabel, components, false);

        for (Standard_Integer i = 1; i <= components.Length(); i++) {
            node.children.emplace_back(std::move(traverse(shapeTool, components.Value(i), partsmap, partsvec, world, level+1)));
        }
    }
    else {
        std::cout << "Local transform:\n";
        DumpTrsf(local);
        std::cout << "World transform:\n";
        DumpTrsf(world);
        std::cout << "XCAF cloal transform:\n";
        TopLoc_Location localLoc = XCAFDoc_ShapeTool::GetLocation(definitionLabel);
        DumpTrsf(localLoc.Transformation());

        glm::dmat4 deflocal = ToGLMTransform(localLoc.Transformation());

        if (partsmap.find(ToUtf8(dentry)) != partsmap.end()){
            node.prototypeIndex = partsmap[ToUtf8(dentry)];
        }
        else{
            // 构造 meshData
            OcctMesher mesher(shape);
            Bbox bbox;
            MeshData meshData = mesher.GetMeshData();
            PartPrototype protyotype;
            protyotype.labelEntry = ToUtf8(dentry);
            protyotype.name = node.name;

            if (meshData.isValid()){
                // TODO: deflocal
                for (auto vert : meshData.vertices) {
                    bbox.expand(vert.position);
                }

                protyotype.definitionShape = shape;
                protyotype.mesh = meshData;
                protyotype.localBbox = bbox;
                protyotype.localTransform = deflocal;
            }

            partsmap[protyotype.labelEntry] = partsvec.size();
            node.prototypeIndex = partsvec.size();
            partsvec.emplace_back(std::move(protyotype));
        }
    }

    return node;
}

StepXcafImporter::StepXcafImporter():
m_application(new TDocStd_Application())
{
    XmlDrivers::DefineFormat(m_application);
    BinDrivers::DefineFormat(m_application);
    BinXCAFDrivers::DefineFormat(m_application);
}

bool StepXcafImporter::load(const std::filesystem::path& path, CadDocument& result){
    // TODO: asset result's vectors are empty
    m_application->NewDocument("BinXCAF", result.m_document);
    if (result.m_document.IsNull()) {
        std::cerr << "Error: Cannot create an BinXCAF document" << std::endl;
        return false;
    }

    // 2. 初始化 Reader 并设置读取选项
    STEPCAFControl_Reader aReader;
    aReader.SetColorMode(true);  // 读取颜色
    aReader.SetNameMode(true);   // 读取名称
    aReader.SetLayerMode(true);  // 读取图层

    // 3. 读取 STEP 文件
    IFSelect_ReturnStatus aStatus = aReader.ReadFile(path.string().c_str());
    if (aStatus != IFSelect_RetDone) {
        std::cerr << "Import Error: Cannot read from "<< path << std::endl;
        return false;        
    }
    // 4. 传输数据到 Document
    Standard_Boolean tStatus = aReader.Transfer(result.m_document);
    if (!tStatus){
        std::cerr << "Import Error: Cannot transfer from "<< path << std::endl;
        return false;      
    }

    // 5. 检查 FreeShapes
    occ::handle<XCAFDoc_ShapeTool> ashapeTool = XCAFDoc_DocumentTool::ShapeTool(result.m_document->Main());
    NCollection_Sequence<TDF_Label> aFreeShapes;
    ashapeTool->GetFreeShapes(aFreeShapes);
    if (aFreeShapes.IsEmpty()) {
        std::cerr<<"Import Error: Invalid document!"<<std::endl;
        return false;
    }

    std::cout<<"Number of FreeShapes: "<<aFreeShapes.Length()<<std::endl<<std::endl;

    // 6. 打印装配树
    result.sourcePath = path;
    std::unordered_map<std::string, uint32_t> prototypes;
    gp_Trsf trsf;
    for (NCollection_Sequence<TDF_Label>::Iterator it(aFreeShapes); it.More(); it.Next()) {
        const TDF_Label& root = it.Value();
        SceneNode node = traverse(ashapeTool, root, prototypes, result.prototypes, trsf, 0);
        result.roots.emplace_back(std::move(node));
    }

    return true;
}
