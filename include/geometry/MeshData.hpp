#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <iostream>
#include <algorithm>

enum class GeoType {
    Triangle
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    // TODO: vertex normal
};

struct MeshData {
    GeoType type = GeoType::Triangle;

    // For triangle
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    bool isValid() const{
        if (indices.size() % 3 != 0) {
            std::cerr << "There are something wrong with triangulation!" << std::endl;
            return false;
        }
        const bool invalidIndex = std::any_of(indices.begin(),indices.end(), [&](uint32_t index) { return index >= vertices.size(); });
        if (invalidIndex) {
            std::cerr << "There are something wrong with triangulation!" << std::endl;
            return false;
        }
        return true;
    }

    bool isRenderable() const{
        return !vertices.empty() && !indices.empty() && isValid();
    }
};