#include "geometry/OcctMesher.hpp"

OcctMesher::OcctMesher(const TopoDS_Shape& shape, double lineDeflection, double angleDeflection):
m_shape(shape)
{
    BRepMesh_IncrementalMesh mesher(
        shape,
        lineDeflection,  // 线性偏差
        false,
        angleDeflection,   // 角度偏差，弧度
        true   // 并行
    );

    mesher.Perform();
    if (!mesher.IsDone()) {
        m_shape = TopoDS_Shape();
        return;
    }
}

void OcctMesher::extractTriangleData(MeshData& data) {
    uint32_t baseVertex = 0;
    for (TopExp_Explorer explorer(m_shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        TopoDS_Face face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        occ::handle<Poly_Triangulation> triangulation = BRep_Tool::Triangulation(face, location);

        if (triangulation.IsNull()) {
            continue;
        }

        const gp_Trsf transform = location.Transformation();
        if (!triangulation->HasNormals()) {
            BRepLib_ToolTriangulatedShape::ComputeNormals(face, triangulation);
        }
        
        for(int i = 1; i <= triangulation->NbNodes(); i++){
            gp_Pnt p = triangulation->Node(i);
            p.Transform(transform);

            auto normal = triangulation->Normal(i);
            if ((face.Orientation() == TopAbs_REVERSED)) {
                normal.Reverse();
            }
            normal.Transform(transform);

            data.vertices.push_back({ {static_cast<float>(p.X()), static_cast<float>(p.Y()), static_cast<float>(p.Z())}, {0.0f, 0.0f, 0.0f} });
            data.vertices.back().normal = glm::normalize(glm::vec3(normal.X(), normal.Y(), normal.Z()));
        }

        for(int triangleIndex = 1; triangleIndex <= triangulation->NbTriangles();triangleIndex++){
            int n1, n2, n3;
            const Poly_Triangle& tri = triangulation->Triangle(triangleIndex);
            tri.Get(n1, n2, n3);

            uint32_t i1 = baseVertex + n1 - 1;
            uint32_t i2 = baseVertex + n2 - 1;
            uint32_t i3 = baseVertex + n3 - 1;

            if (face.Orientation() == TopAbs_REVERSED) {
                std::swap(i2, i3);
            }

            data.indices.push_back(i1);
            data.indices.push_back(i2);
            data.indices.push_back(i3);
        }

        baseVertex += triangulation->NbNodes();
    }
}

MeshData OcctMesher::GetMeshData(){
    MeshData data;

    if (m_shape.IsNull()){
        return data;
    }

    data.type = GeoType::Triangle;
    extractTriangleData(data);

    return data;
}