#include "render/ShaderProgram.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <utility>

ShaderProgram::ShaderProgram(){
    m_program = glCreateProgram();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept:
m_program(std::exchange(other.m_program, 0)) 
{
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other){
        clear();
        this->m_program = std::exchange(other.m_program, 0);
    }
    return *this;
}

ShaderProgram::~ShaderProgram(){
    clear();
}

void ShaderProgram::clear(){
    if (m_program)
    glDeleteProgram(m_program);
    m_program = 0;
}

bool ShaderProgram::LoadShader(const std::string& vert, const std::string& frag){
    bool result = true;

    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;
    // ensure ifstream objects can throw exceptions:
    vShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
    try 
    {
        // open files
        vShaderFile.open(vert);
        fShaderFile.open(frag);
        std::stringstream vShaderStream, fShaderStream;
        // read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();		
        // close file handlers
        vShaderFile.close();
        fShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();			
    }
    catch (std::ifstream::failure& e)
    {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        result = false;
    }

    GLuint vertshader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragshader = glCreateShader(GL_FRAGMENT_SHADER);

    const char* vShaderCode = vertexCode.c_str();
    const char * fShaderCode = fragmentCode.c_str();

    glShaderSource(vertshader, 1, &vShaderCode, nullptr);
    glCompileShader(vertshader);
    if (!checkError(vertshader, CheckType::vertex)){
        result = false;
        std::cout<<"at "<<vert<<std::endl;
    }

    glShaderSource(fragshader, 1, &fShaderCode, nullptr);
    glCompileShader(fragshader);
    if (!checkError(fragshader, CheckType::fragment)){
        result = false;
        std::cout<<"at "<<frag<<std::endl;
    }

    glAttachShader(m_program, vertshader);
    glAttachShader(m_program, fragshader);
    glLinkProgram(m_program);
    if (!checkError(m_program, CheckType::program)){
        result = false;
    }

    glDeleteShader(vertshader);
    glDeleteShader(fragshader);
    return result;
}

GLuint ShaderProgram::id() const{
    return m_program;
}

void ShaderProgram::use(){
    glUseProgram(m_program);
}

void ShaderProgram::setUint(const std::string& name, const uint32_t& value) const {
    GLuint loc = glGetUniformLocation(m_program, name.c_str());
    glUniform1ui(loc, value);
}

void ShaderProgram::setInt(const std::string& name, const int& value) const {
    GLuint loc = glGetUniformLocation(m_program, name.c_str());
    glUniform1i(loc, value);
}

void ShaderProgram::setMat4(const std::string& name, const glm::mat4& mat) const{
    GLuint loc = glGetUniformLocation(m_program, name.c_str());
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mat));
}

void ShaderProgram::setVec3(const std::string& name, const glm::vec3& vec) const{
    GLuint loc = glGetUniformLocation(m_program, name.c_str());
    glUniform3f(loc, vec.x, vec.y, vec.z);
}

void ShaderProgram::setVec4(const std::string& name, const glm::vec4& vec) const{
    GLuint loc = glGetUniformLocation(m_program, name.c_str());
    glUniform4f(loc, vec.x, vec.y, vec.z, vec.w);
}


bool ShaderProgram::checkError(GLuint obj, CheckType type){
    GLint success;
    GLchar infoLog[1024];

    if (type < CheckType::shader){
        glGetShaderiv(obj, GL_COMPILE_STATUS, &success);
        if (!success){
            glGetShaderInfoLog(obj, 1024, NULL, infoLog);
            // TODO: use a function?
            std::string shadertype = type == CheckType::vertex ? "Vertex" : "Shader";
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << shadertype << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            return false;
        }
    }
    else{
        glGetProgramiv(obj, GL_LINK_STATUS, &success);
        if (!success){
            glGetProgramInfoLog(obj, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            return false;
        }
    }

    return true;
}