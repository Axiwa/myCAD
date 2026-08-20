#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <Windows.h>
#include <string>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "scene/Transform.hpp"
#include "view/Controller.hpp"
#include "geometry/MeshData.hpp"
#include "geometry/OcctMesher.hpp"

std::filesystem::path executableDirectory()
{
    std::wstring buffer(32768, L'\0');

    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error(
            "Failed to get executable path"
        );
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

char* filetobuf(const char* file)
{
    FILE* fptr;
    long length;
    char* buf;

    fptr = fopen(file, "rb"); /* Open file for reading */
    if (!fptr) /* Return NULL on failure */
        return NULL;
    fseek(fptr, 0, SEEK_END); /* Seek to the end of the file */
    length = ftell(fptr); /* Find out how many bytes into the file we are */
    buf = (char*)malloc(length + 1); /* Allocate a buffer for the entire length of the file and a null terminator */
    fseek(fptr, 0, SEEK_SET); /* Go back to the beginning of the file */
    fread(buf, length, 1, fptr); /* Read the contents of the file in to the buffer */
    fclose(fptr); /* Close the file */
    buf[length] = 0; /* Null terminator */

    return buf; /* Return the buffer */
}

bool LoadShader(const char* vertfile, const char* fragfile, GLuint& vertshader, GLuint& fragshader){
    if (vertshader != 0){
        glDeleteShader(vertshader);
    }
    if (fragshader != 0){
        glDeleteShader(fragshader);
    }

    int success;
    int maxLength;

    GLchar* vertsource;
    GLchar* fragsource;

    vertsource = filetobuf(vertfile);
    fragsource = filetobuf(fragfile);

    vertshader = glCreateShader(GL_VERTEX_SHADER);
    fragshader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertshader, 1, (const GLchar**)&vertsource, 0);
    glShaderSource(fragshader, 1, (const GLchar**)&fragsource, 0);

    glCompileShader(vertshader);
    glGetShaderiv(vertshader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        glGetShaderiv(vertshader, GL_INFO_LOG_LENGTH, &maxLength);
        char* vertexInfoLog;

        /* The maxLength includes the NULL character */
        vertexInfoLog = (char*)malloc(maxLength);
        glGetShaderInfoLog(vertshader, maxLength, &maxLength, vertexInfoLog);
        std::cout << vertexInfoLog << std::endl;
        free(vertexInfoLog);
        return false;
    }

    glCompileShader(fragshader);
    glGetShaderiv(fragshader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        glGetShaderiv(fragshader, GL_INFO_LOG_LENGTH, &maxLength);
        char* fragInfoLog;

        fragInfoLog = (char*)malloc(maxLength);
        glGetShaderInfoLog(fragshader, maxLength, &maxLength, fragInfoLog);
        std::cout << fragInfoLog << std::endl;
        free(fragInfoLog);
        return false;
    }

    return true;
}

gp_Trsf ToOcctTransform(const glm::dmat4& m) {
    gp_Trsf T;
    gp_Mat rot(
        m[0][0], m[0][1], m[0][2],
        m[1][0], m[1][1], m[1][2],
        m[2][0], m[2][1], m[2][2]
    );
    
    T.SetValues(
        rot.Value(1, 1), rot.Value(1, 2), rot.Value(1, 3), m[3][0],
        rot.Value(2, 1), rot.Value(2, 2), rot.Value(2, 3), m[3][1],
        rot.Value(3, 1), rot.Value(3, 2), rot.Value(3, 3), m[3][2]
    );

    return T;
}

glm::dmat4 ToGLMTransform(const gp_Trsf& T) {
    glm::dmat4 m(1.0);
    for (int i = 1; i < 4; i++) {
        for (int j = 1; j <= 4; j++) {
            m[j - 1][i - 1] = T.Value(i, j);
        }
    }
    return m;
}

int main(){
    //BRepPrimAPI_MakeBox mkBox(1., 1., 1);
    BRepPrimAPI_MakeCylinder mkCylinder(1.0, 2.0);
    OcctMesher mesher(mkCylinder); // mkBox
    MeshData meshData = mesher.GetMeshData();
    if (meshData.indices.size() % 3 != 0) {
        std::cerr << "There are something wrong with triangulation!" << std::endl;
        exit(1);
    }
    const bool invalidIndex = std::any_of(meshData.indices.begin(), meshData.indices.end(), [&](uint32_t index) {
        return index >= meshData.vertices.size();
        });
    if (invalidIndex) {
        std::cerr << "There are something wrong with triangulation!" << std::endl;
        exit(1);
    }

    Bbox bbox;

    Controller myWindow;
    glfwMakeContextCurrent(myWindow.GetWindow());

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        exit(1);
    }

    GLuint program;
    program = glCreateProgram();
    GLuint vertshader = 0, fragshader = 0;
    std::string vertpath = executableDirectory().string() + "/shader/plain.vert";
    std::string fragpath = executableDirectory().string() + "/shader/plain.frag";
    LoadShader(vertpath.c_str(), fragpath.c_str(), vertshader, fragshader);

    glAttachShader(program, vertshader);
    glAttachShader(program, fragshader);
    glLinkProgram(program);

    glDeleteShader(vertshader);
    glDeleteShader(fragshader);

    int success;
    int maxLength;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        char* progInfoLog;

        progInfoLog = (char*)malloc(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, progInfoLog);
        std::cout << progInfoLog << std::endl;
        free(progInfoLog);
        return false;
    }

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(Vertex), meshData.vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.size() * sizeof(uint32_t), meshData.indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    Transform modeltrs;
    modeltrs.updateAsRoot();
    glm::mat4 model = modeltrs.worldMatrix();
    

    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    view = glm::lookAt(glm::vec3(3,3,3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_DEPTH_TEST);
    bool isPressed = false;
    bool wasPressed = false;
    float time = 0;
    while(!glfwWindowShouldClose(myWindow.GetWindow())){
        glfwPollEvents();

        if (!myWindow.update()) {
            glfwWaitEvents();
            continue;
        }
        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(VAO);

        const GLint modelLocation = glGetUniformLocation(program, "model");
        const GLint viewLocation = glGetUniformLocation(program, "view");
        const GLint projectionLocation = glGetUniformLocation(program, "proj");

        // Temp code for testing normal
        glm::mat4 modelrun;
        {
            modelrun = glm::rotate(model, glm::radians(time), glm::vec3(1.0f, 0.f, 1.f));
            modelrun = glm::scale(modelrun, glm::vec3(1.0f, 1.0f, 2.0f));
            modelrun = glm::translate(modelrun, glm::vec3(-0.5f, -0.5f, -0.5f));

            for (auto vert : meshData.vertices) {
                glm::vec3 newvert = modelrun * glm::vec4(vert.position, 1);
                bbox.expand(newvert);
            }
        }

        glUniformMatrix4fv(modelLocation, 1, false, glm::value_ptr(modelrun));
        glUniformMatrix4fv(viewLocation, 1, false, glm::value_ptr(myWindow.GetCamera()->viewMatrix()));
        glUniformMatrix4fv(projectionLocation, 1, false, glm::value_ptr(myWindow.GetCamera()->projectionMatrix()));

        glDrawElements(GL_TRIANGLES, meshData.indices.size(), GL_UNSIGNED_INT, nullptr);
        // glDrawArrays(GL_TRIANGLES, 0, vertices.size());
        //glBindVertexArray(0);

        isPressed = (glfwGetKey(myWindow.GetWindow(), GLFW_KEY_F) == GLFW_PRESS);
        if (isPressed && !wasPressed) {
            myWindow.GetCamera()->fitToBbox(bbox);
        }

        wasPressed = isPressed;
        glfwSwapBuffers(myWindow.GetWindow());
        time++;
    }

    return 0;
}