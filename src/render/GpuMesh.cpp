#include "render/GpuMesh.hpp"

#include<iostream>
#include <limits>

GpuMesh::GpuMesh(){
    glGenVertexArrays(1, &VAO);
    glGenVertexArrays(1, &VAOInstanced);
}

GpuMesh::GpuMesh(GpuMesh&& other) noexcept:
VAO(std::exchange(other.VAO, 0)),
VBO(std::exchange(other.VBO, 0)),
EBO(std::exchange(other.EBO, 0)),
VAOInstanced(std::exchange(other.VAOInstanced, 0)),
m_indexCount(std::exchange(other.m_indexCount, 0)){

}

GpuMesh& GpuMesh::operator=(GpuMesh&& other) noexcept{
    if (this != &other){
        clear();
        VAO = std::exchange(other.VAO, 0);
        VBO = std::exchange(other.VBO, 0);
        EBO = std::exchange(other.EBO, 0);
        VAOInstanced = std::exchange(other.VAOInstanced, 0);
        m_indexCount = std::exchange(other.m_indexCount, 0);
    }

    return *this;
}

void GpuMesh::upload(const MeshData& meshdata){
    if (meshdata.vertices.empty() || meshdata.indices.empty() || !meshdata.isValid()){
        std::cerr<<"Empty object! No GPU resources created"<<std::endl;
        return;
    }
    if (meshdata.indices.size() >
        static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        std::cerr << "Too many indices for one glDrawElements call\n";
        return;
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

    m_indexCount = static_cast<GLsizei>(meshdata.indices.size());
    
    unbind();

    bindInstance();

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);


    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);

    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    
    unbind();
}

void GpuMesh::setInstance(GLuint& instanceVBO){
    bindInstance();

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    unbind();
}

void GpuMesh::bind(){
    glBindVertexArray(VAO);
}

void GpuMesh::unbind(){
    glBindVertexArray(0);
}

void GpuMesh::bindInstance(){
    glBindVertexArray(VAOInstanced);
}

void GpuMesh::draw(){
    if (!isValid()){
        std::cerr<<"Invalid resources"<<std::endl;
        return;
    }
    bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    unbind();
}

void GpuMesh::drawInstance(uint32_t count){
    if (!isValid()){
        std::cerr<<"Invalid resources"<<std::endl;
        return;
    }
    bindInstance();
    glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr, count);
    unbind();
}

bool GpuMesh::isValid(){
    if (VAO == 0 || VAOInstanced == 0 || VBO == 0 || EBO == 0){
        return false;
    }
    return true;
}

GpuMesh::~GpuMesh(){
    clear();
}

void GpuMesh::clear(){
    if (VAO!=0){
        glDeleteVertexArrays(1, &VAO);
    }
    if (VAOInstanced != 0) {
        glDeleteVertexArrays(1, &VAOInstanced);
    }
    if (VBO != 0){
        glDeleteBuffers(1, &VBO);
    }
    if (EBO != 0){
        glDeleteBuffers(1, &EBO);
    }

    VAO = 0;
    VBO = 0;
    EBO = 0;
    VAOInstanced = 0;
    m_indexCount = 0;
}