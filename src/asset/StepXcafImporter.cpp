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
#include <BRepBuilderAPI_Transform.hxx>
#include <iostream>
#include <string>
#include <unordered_map>
#include <filesystem>

extern bool isFaceMirror;
extern bool isNodeMirror;

static void DumpTrsf(const gp_Trsf& trsf) {
    for (int i = 1; i < 4; i++) {
        for (int j = 1; j <= 4; j++) {
            std::cout << trsf.Value(i, j)<<" ";
        }
        std::cout << std::endl;
    }
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

static bool ApplySurfaceColor(
    const occ::handle<XCAFDoc_ColorTool>& colorTool,
    const TDF_Label& label,
    XCAFDoc_ColorType type,
    ColorSource source,
    ResolvedStyle& style)
{
    Quantity_ColorRGBA color;
    if (!colorTool->GetColor(label, type, color)) {
        return false;
    }

    style.surfaceColor = glm::vec4(color.GetRGB().Red(), color.GetRGB().Green(), color.GetRGB().Blue(), color.Alpha());;
    style.colorSource = source;
    return true;
}

static std::string ToUtf8(const TCollection_ExtendedString& s)
{
    const int len = s.LengthOfCString();
    std::string out(len, '\0');
    auto data = out.data();
    s.ToUTF8CString(data);
    return out;
}

static std::string GetLabelName(const TDF_Label& label){
    occ::handle<TDataStd_Name> name;
    if (label.FindAttribute(TDataStd_Name::GetID(), name)) {
        return ToUtf8(name->Get());
    }
    else{
        return std::string();
    }
}

SceneNode traverse(const occ::handle<XCAFDoc_ShapeTool>& shapeTool, const occ::handle<XCAFDoc_ColorTool>& colorTool, const TDF_Label& root, std::unordered_map<std::string, uint32_t>& partsmap, std::vector<PartPrototype>& partsvec, const gp_Trsf& parent, const ResolvedStyle& parentcolor, int level, SceneNodeId& nodeId) {
    SceneNode node;
    node.colorStyle = parentcolor;
    nodeId++;
    node.id = nodeId;

    TDF_Label occurrenceLabel = root;
    TDF_Label definitionLabel = root;

    std::cout << "--------------------------------" << std::endl;
    std::cout << "Transform Level: " << level << "\n";

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
    }

    ApplySurfaceColor(colorTool, definitionLabel, XCAFDoc_ColorGen, ColorSource::DefinitionGeneral, node.colorStyle);
    ApplySurfaceColor(colorTool, definitionLabel, XCAFDoc_ColorSurf, ColorSource::DefinitionSurface, node.colorStyle);
    if (definitionLabel != occurrenceLabel) {
        ApplySurfaceColor(colorTool, occurrenceLabel, XCAFDoc_ColorGen, ColorSource::OccurrenceGeneral, node.colorStyle);
        ApplySurfaceColor(colorTool, occurrenceLabel, XCAFDoc_ColorSurf, ColorSource::OccurrenceSurface, node.colorStyle);
    }

    TDF_Tool::Entry(occurrenceLabel, entry);
    TDF_Tool::Entry(definitionLabel, dentry);
    node.occLabelEntry = entry.ToCString();

    const std::string definitionName = GetLabelName(definitionLabel);
    const std::string occurrenceName = occurrenceLabel != definitionLabel ? GetLabelName(occurrenceLabel) : std::string{};
    node.name = occurrenceName.empty() ? definitionName : occurrenceName;

    TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(definitionLabel);

    const TopLoc_Location location = XCAFDoc_ShapeTool::GetLocation(occurrenceLabel);
    gp_Trsf local = location.Transformation();
    gp_Trsf world = parent * local;
    node.localTransform = ToGLMTransform(local);
    node.worldTransform = ToGLMTransform(world);

    // === 测试 occurrence transform ===
    if (isNodeMirror){
        if (entry == "0:1:1:2:2") {
            gp_Trsf testOffset, testMirror;
            testMirror.SetMirror(gp::YOZ());
            testOffset.SetTranslation(gp_Vec(150, 0, 0));

            local = testOffset * local * testMirror;
            world = parent * local;
            node.localTransform = ToGLMTransform(local);
            node.worldTransform = ToGLMTransform(world);
        }
    }

    std::cout<< "SceneNode name: " << node.name << std::endl;
    std::cout<< "SceneNode id: " << node.id << std::endl;
    std::cout << "Occurrence Label entry: " << entry << std::endl;
    std::cout << "Color Source: " << node.colorStyle.toSourceString() << std::endl;
    std::cout << "RGBA (" << node.colorStyle.surfaceColor.r << " " << node.colorStyle.surfaceColor.g << " " << node.colorStyle.surfaceColor.b << " " << node.colorStyle.surfaceColor.a << ")" << std::endl;
    if (!shape.IsNull())
    {
        std::cout << "Shape type: " << (int)shape.ShapeType() << "\n";
    }
    else
    {
        std::cout << "Shape: <none>\n";
    }
    std::cout << "SceneNode mirrored: " << (glm::determinant(glm::dmat3(node.worldTransform)) < 0.0) << std::endl;
    std::cout << "Definition Label entry: " << dentry << std::endl;
    std::cout << "Prototype name: " << definitionName << std::endl;
    std::cout << "Tag: " << definitionLabel.Tag() << "\n";
    std::cout << "Depth: " << definitionLabel.Depth() << "\n";
    
    if (XCAFDoc_ShapeTool::IsAssembly(definitionLabel)) {
        NCollection_Sequence<TDF_Label> components;
        shapeTool->GetComponents(definitionLabel, components, false);

        for (int i = 1; i <= components.Length(); i++) {
            node.children.emplace_back(traverse(shapeTool, colorTool, components.Value(i), partsmap, partsvec, world, node.colorStyle, level+1, nodeId));
        }
    }
    else {
        std::cout<< "---" << std::endl << "Local transform:\n";
        DumpTrsf(local);
        std::cout << "World transform:\n";
        DumpTrsf(world);
        std::cout << "XCAF cloal transform:\n";
        TopLoc_Location localLoc = XCAFDoc_ShapeTool::GetLocation(definitionLabel);
        DumpTrsf(localLoc.Transformation());

        if (partsmap.find(ToUtf8(dentry)) != partsmap.end()){
            node.prototypeIndex = partsmap[ToUtf8(dentry)];
        }
        else{
            gp_Trsf mirrorTrsf;
            // ==== 测试transform代码 ====
            if (isFaceMirror){
                mirrorTrsf.SetMirror(gp::XOY());
            }
            // ==========================
            TopoDS_Shape testShape = shape.Moved(TopLoc_Location(mirrorTrsf), false);

            OcctMesher mesher(testShape);
            Bbox bbox;
            MeshData meshData = mesher.GetMeshData();

            PartPrototype prototype;
            prototype.labelEntry = ToUtf8(dentry);
            prototype.name = definitionName.empty() ? ToUtf8(dentry) : definitionName;
            prototype.definitionShape = testShape;

            if (!meshData.isValid()) {
                prototype.meshStatus = MeshStatus::Invalid;
            }
            else if (meshData.isRenderable()){
                for (auto vert : meshData.vertices) {
                    bbox.expand(vert.position);
                }
                prototype.mesh = meshData;
                prototype.meshStatus = MeshStatus::Ready;
                prototype.localBbox = bbox;
            }

            partsmap[prototype.labelEntry] = static_cast<uint32_t>(partsvec.size());
            node.prototypeIndex = partsvec.size();
            partsvec.emplace_back(std::move(prototype));
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
    result.clear();

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
    if ( !std::filesystem::exists(path)){
        std::cerr << "Import Error: Path does not exist: "<< path << std::endl;
        return false;   
    }
    IFSelect_ReturnStatus aStatus = aReader.ReadFile(path.string().c_str());
    if (aStatus != IFSelect_RetDone) {
        std::cerr << "Import Error: Cannot read from "<< path << std::endl;
        return false;        
    }
    // 4. 传输数据到 Document
    bool tStatus = aReader.Transfer(result.m_document);
    if (!tStatus){
        std::cerr << "Import Error: Cannot transfer from "<< path << std::endl;
        return false;      
    }

    // 5. 检查 FreeShapes
    occ::handle<XCAFDoc_ShapeTool> shapeTool = XCAFDoc_DocumentTool::ShapeTool(result.m_document->Main());
    NCollection_Sequence<TDF_Label> aFreeShapes;
    shapeTool->GetFreeShapes(aFreeShapes);
    if (aFreeShapes.IsEmpty()) {
        std::cerr<<"Import Error: Invalid document!"<<std::endl;
        return false;
    }

    std::cout<<"Number of FreeShapes: "<<aFreeShapes.Length()<<std::endl<<std::endl;

    occ::handle<XCAFDoc_ColorTool> colorTool = XCAFDoc_DocumentTool::ColorTool(result.m_document->Main());
    // 6. 打印装配树
    result.sourcePath = path;
    std::unordered_map<std::string, uint32_t> prototypes;
    gp_Trsf trsf;
    ResolvedStyle color;
    SceneNodeId nodeId = 0;
    for (NCollection_Sequence<TDF_Label>::Iterator it(aFreeShapes); it.More(); it.Next()) {
        const TDF_Label& root = it.Value();
        SceneNode node = traverse(shapeTool, colorTool, root, prototypes, result.prototypes, trsf, color, 0, nodeId);
        result.roots.emplace_back(std::move(node));
    }
    result.numNodes = nodeId;

    std::cout << "--------------------------------" << std::endl;
    std::cout << "Prototypes: " << std::endl;
    for(auto& prototype: result.prototypes){
        bool isMirroredShape = prototype.definitionShape.Location().Transformation().IsNegative();
        std::cout << "--- " << std::endl;
        std::cout << "Label entry: " << prototype.labelEntry << "\n";
        std::cout << "Prototype name: " << prototype.name << "\n";
        std::cout << "Prototype mirrored: " << isMirroredShape << "\n";
        std::cout << "Mesh Status: " << prototype.toMeshStatusString() << "\n";
    }

    return true;
}
