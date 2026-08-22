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
#include "render/ShaderProgram.hpp"
#include "render/GpuMesh.hpp"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

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
    if (!meshData.isValid()){
        exit(1);
    }

    Controller myWindow;
    glfwMakeContextCurrent(myWindow.GetWindow());

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        exit(1);
    }

    ShaderProgram shaderProgram;
    std::string vertpath = executableDirectory().string() + "/shader/plain.vert";
    std::string fragpath = executableDirectory().string() + "/shader/plain.frag";
    if (!shaderProgram.LoadShader(vertpath, fragpath)){
        exit(1);
    }

    GpuMesh meshbuffer;
    meshbuffer.upload(meshData, shaderProgram.id());

    Transform modeltrs;
    modeltrs.updateAsRoot();
    glm::mat4 model = modeltrs.worldMatrix();
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    view = glm::lookAt(glm::vec3(3,3,3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    model = glm::rotate(model, glm::radians(30.0f), glm::vec3(1.0f, 0.f, 1.f));
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 2.0f));
    model = glm::translate(model, glm::vec3(-0.5f, -0.5f, -0.5f));

    Bbox bbox;

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_DEPTH_TEST);
    bool isPressed = false;
    bool wasPressed = false;

    while(!glfwWindowShouldClose(myWindow.GetWindow())){
        glfwPollEvents();

        if (!myWindow.update()) {
            glfwWaitEvents();
            continue;
        }
        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shaderProgram.use();
        meshbuffer.bind();

        for (auto vert : meshData.vertices) {
            glm::vec3 newvert = model * glm::vec4(vert.position, 1);
            bbox.expand(newvert);
        }

        shaderProgram.setMat4("model", model);
        shaderProgram.setMat4("view", myWindow.GetCamera()->viewMatrix());
        shaderProgram.setMat4("proj", myWindow.GetCamera()->projectionMatrix());

        meshbuffer.draw();
        meshbuffer.unbind();

        isPressed = (glfwGetKey(myWindow.GetWindow(), GLFW_KEY_F) == GLFW_PRESS);
        if (isPressed && !wasPressed) {
            myWindow.GetCamera()->fitToBbox(bbox);
        }
        wasPressed = isPressed;
        glfwSwapBuffers(myWindow.GetWindow());
    }

    return 0;
}