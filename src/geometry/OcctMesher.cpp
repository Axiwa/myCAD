#include "geometry/OcctMesher.hpp"

OcctMesher::OcctMesher(const TopoDS_Shape& shape, double lineDeflection, double angleDeflection):
m_shape(shape),
m_linedeflection(lineDeflection),
m_angledeflection(angleDeflection)
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
    
    m_transform = m_shape.Location().Transformation();
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

        for(int i = 1; i <= triangulation->NbNodes(); i++){
            gp_Pnt p = triangulation->Node(i);
            p.Transform(transform);

            data.vertices.push_back({ {static_cast<float>(p.X()), static_cast<float>(p.Y()), static_cast<float>(p.Z())}, {0.0f, 0.0f, 0.0f} });
            if (!triangulation->HasNormals()) {
                BRepLib_ToolTriangulatedShape::ComputeNormals(face, triangulation);
            }
            auto normal = triangulation->Normal(i);
            const bool isMirrored = transform.VectorialPart().Determinant() < 0.0;
            if ((face.Orientation() == TopAbs_REVERSED) ^ isMirrored) {
                normal.Reverse();
            }
            normal.Transform(transform);
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

            // Calculate vertex normal if needed
            //if (!hasNormal) {
                //gp_Pnt p1 = triangulation->Node(n1);
                //gp_Pnt p2 = triangulation->Node(n2);
                //gp_Pnt p3 = triangulation->Node(n3);
                //p1.Transform(transform);
                //p2.Transform(transform);
                //p3.Transform(transform);
                //gp_Vec v1(p1, p2);
                //gp_Vec v2(p2, p3);
                //gp_Vec v3(p3, p1);
                //gp_Vec n = v1.Crossed(v2);
                //if (face.Orientation() == TopAbs_REVERSED) {
                //    n.Reverse();
                //}
                //glm::vec3 normal = glm::normalize(glm::vec3(n.X(), n.Y(), n.Z()));
                //data.vertices[i1].normal = normal;
                //data.vertices[i2].normal = normal;
                //data.vertices[i3].normal = normal;
            //}
        }

        baseVertex += triangulation->NbNodes();
    }
}

MeshData OcctMesher::GetMeshData(){
    MeshData data;

    if (m_shape.IsNull()){
        return data;
    }

    TopAbs_ShapeEnum type = m_shape.ShapeType();

    if (type == TopAbs_FACE || type == TopAbs_SHELL || type == TopAbs_SOLID){
        data.type = GeoType::Triangle;
        extractTriangleData(data);
    }

    return data;
}