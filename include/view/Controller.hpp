#pragma once
#include <GLFW/glfw3.h>
#include "view/OrbitCamera.hpp"
#include <memory>

struct Inputstate {
    double mouseX = 0.0;
    double mouseY = 0.0;

    double previousMouseX = 0.0;
    double previousMouseY = 0.0;

    double scrollDelta = 0.0;

    bool middleButton = false;
    bool rightButton = false;
    bool shift = false;
};

class Controller{
public:
    Controller();
    ~Controller();

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    bool update();
    GLFWwindow* GetWindow();
    OrbitCamera* GetCamera();

private:
    GLFWwindow* m_window;
    OrbitCamera* m_camera;
    Inputstate m_input;
};