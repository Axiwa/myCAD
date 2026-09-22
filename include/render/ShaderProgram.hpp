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
    void setUint(const std::string& name, const uint32_t& value) const;
    void setInt(const std::string& name, const int& value) const;
    void setFloat(const std::string& name, const float& value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setVec3(const std::string& name, const glm::vec3& vec3) const;
    void setVec4(const std::string& name, const glm::vec4& vec4) const;

    GLuint id() const;
private:
    GLuint m_program = 0;
    bool checkError(GLuint obj, CheckType type);
    void clear();
};