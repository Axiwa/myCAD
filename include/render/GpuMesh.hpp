#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include "geometry/MeshData.hpp"

class GpuMesh{
public:
    GpuMesh();
    GpuMesh(const GpuMesh& other) = delete;
    GpuMesh(GpuMesh&& other) noexcept;
    GpuMesh& operator=(const GpuMesh&) = delete;
    GpuMesh& operator=(GpuMesh&& other) noexcept;
    ~GpuMesh();
    void upload(const MeshData& meshdata);
    void uploadInstance(std::vector<glm::mat4>& mats);
    void bind();
    void unbind();
    void bindInstance();
    void draw();
    void drawInstance(uint32_t count);
    bool isValid();

private:
    GLuint VAO = 0;
    GLuint VAOInstanced = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLuint IBO = 0;
    GLsizei m_indexCount = 0;
    void clear();
};