#pragma once

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

enum CheckType{
    vertex,
    fragment,
    shader,
    program
};

class ShaderProgram{
public:
    ShaderProgram();
    ShaderProgram(const ShaderProgram& other) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(const ShaderProgram& other) = delete;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    ~ShaderProgram();

    bool LoadShader(const std::string& vert, const std::string& frag);
    
    void use();
    void setMat4(const std::string& name, const glm::mat4& mat) const;

    GLuint id() const;
private:
    GLuint m_program = 0;
    bool checkError(GLuint obj, CheckType type);
    void clear();
};