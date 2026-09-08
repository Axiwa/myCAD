#include "view/Controller.hpp"
#include <imgui.h>
#include <iostream>

void Controller::FramebufferSizeCallback(GLFWwindow* window, int width, int height){
    Controller* myself = reinterpret_cast<Controller*>(glfwGetWindowUserPointer(window));
    if (width <= 0 || height <= 0){
        return;
    }
    glViewport(0, 0, width, height);

    myself->m_camera->setViewport(width, height);
    myself->m_pickBuffer->resize(width, height);
}

void Controller::ScrollCallback(GLFWwindow* window, double xOffset, double yOffset){
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    Controller* myself = reinterpret_cast<Controller*>(glfwGetWindowUserPointer(window));
    if (yOffset < 0){
        myself->m_camera->dolly(1.1f);
    }
    else{
        myself->m_camera->dolly(1 / 1.1f);
    }
}

void Controller::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods){
    Controller* myself = reinterpret_cast<Controller*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_RIGHT){
        if (action == GLFW_PRESS){
            myself->m_input.rightButton = true;
        }
        else{
            myself->m_input.rightButton = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE){
        if (action == GLFW_PRESS){
            myself->m_input.middleButton = true;
        }
        else{
            myself->m_input.middleButton = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            myself->m_input.leftButton = true;
        }
        else {
            myself->m_input.leftButton = false;
        }
    }
    myself->m_input.previousMouseX = myself->m_input.mouseX;
    myself->m_input.previousMouseY = myself->m_input.mouseY;
}

void Controller::CursorPosCallback(GLFWwindow* window, double xpos, double ypos){
    Controller* myself = reinterpret_cast<Controller*>(glfwGetWindowUserPointer(window));
    myself->m_input.mouseX = xpos;
    myself->m_input.mouseY = ypos;
}

Controller::Controller(){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int width = 1024, height = 1024;
    m_window = glfwCreateWindow(width, height, "Scene", nullptr, nullptr);
    if (m_window == nullptr){ glfwTerminate(); }

    m_camera = new OrbitCamera(width, height);
    m_pickBuffer = new PickingFrameBuffer();

    glfwMakeContextCurrent(m_window);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_window, CursorPosCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
}

Controller::~Controller(){
    if (m_window){
        glfwDestroyWindow(m_window);
    }
    if (m_camera){
        delete m_camera;
    }
    if (m_pickBuffer){
        delete m_pickBuffer;
    }

    glfwTerminate();
}

bool Controller::update(bool allowCameraInput){
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    if (width <= 0 || height <= 0){
        return false;
    }

    double dx = m_input.mouseX - m_input.previousMouseX;
    double dy = m_input.mouseY - m_input.previousMouseY;

    if (allowCameraInput) {
        // dolly has been processed
        // pan
        if (m_input.middleButton && glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            m_camera->pan(dx, dy);
        }
        // orbit
        if (m_input.rightButton) {
            float dphi = -dx * glm::two_pi<float>() / width;
            float dtheta = -dy * glm::pi<float>() / height;
            m_camera->orbit(dphi, dtheta);
        }

        // pick
        if (m_input.leftButton) {
            int w_width, w_height;
            glfwGetWindowSize(m_window, &w_width, &w_height);

            float scaleX = width / w_width;
            float scaleY = height / w_height;

            int pixelX = floor(m_input.mouseX * scaleX);
            int pixelY = height - 1 - floor(m_input.mouseY * scaleY);

            std::cout << "My pick id: " << m_pickBuffer->pickID(pixelX, pixelY) << std::endl;
        }
    }

    m_input.previousMouseX = m_input.mouseX;
    m_input.previousMouseY = m_input.mouseY;

    return true;
}

GLFWwindow* Controller::GetWindow(){
    return m_window;
}

OrbitCamera* Controller::GetCamera(){
    return m_camera;
}

PickingFrameBuffer* Controller::GetPickFramebuffer(){
    return m_pickBuffer;
}

void Controller::InitPickframebuffer() {
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }
    m_pickBuffer->resize(width, height);
}

bool Controller::bindPickBuffer(){
    if (m_pickBuffer->bind()) return true;
    return false;
}

void Controller::unbindPickBuffer(GLuint origFB){
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, origFB);
    glViewport(0, 0, width, height);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}