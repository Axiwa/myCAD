#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

enum class GeoType {
    Triangle
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    // TODO: vertex normal
};

struct MeshData {
    GeoType type;

    // For triangle
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};