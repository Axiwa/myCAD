#include "render/GpuMesh.hpp"

#include<iostream>

GpuMesh::GpuMesh(){
    glGenVertexArrays(1, &VAO);
}

GpuMesh::GpuMesh(GpuMesh&& other) noexcept{
    VAO = other.VAO;
    VBO = other.VBO;
    EBO = other.EBO;
    m_indexCount = other.m_indexCount;

    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
}

GpuMesh& GpuMesh::operator=(GpuMesh&& other) noexcept{
    if (this != &other){
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        m_indexCount = other.m_indexCount;
    }

    return *this;
}

void GpuMesh::upload(const MeshData& meshdata, GLuint program){
    if (meshdata.vertices.empty() || meshdata.indices.empty() || !meshdata.isValid()){
        std::cerr<<"Empty object! No GPU resources created"<<std::endl;
    }

    bind();

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, meshdata.vertices.size() * sizeof(Vertex), meshdata.vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // We don't consider meshdata without indices
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshdata.indices.size() * sizeof(uint32_t), meshdata.indices.data(), GL_STATIC_DRAW);

    m_indexCount = meshdata.indices.size();
    
    unbind();
}

void GpuMesh::bind(){
    glBindVertexArray(VAO);
}

void GpuMesh::unbind(){
    glBindVertexArray(0);
}

void GpuMesh::draw(){
    if (!isValid()){
        std::cerr<<"Invalid resources"<<std::endl;
        return;
    }
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
}

bool GpuMesh::isValid(){
    if (VAO == 0 || VBO == 0 || EBO == 0){
        return false;
    }
    return true;
}

GpuMesh::~GpuMesh(){
    if (VAO!=0){
        glDeleteVertexArrays(1, &VAO);
    }
    if (VBO != 0){
        glDeleteBuffers(1, &VBO);
    }
    if (EBO != 0){
        glDeleteBuffers(1, &VBO);
    }
}