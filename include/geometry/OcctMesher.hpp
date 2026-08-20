#pragma once

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include "geometry/MeshData.hpp"

class OcctMesher {
public:
    OcctMesher(const TopoDS_Shape& shape, double lineDeflection = 0.01, double angleDeflection = 0.5);

    MeshData GetMeshData();
    void extractTriangleData(MeshData& data);

private:
    TopoDS_Shape m_shape;
    Standard_Real m_linedeflection;
    Standard_Real m_angledeflection;
    gp_Trsf m_transform;
};
