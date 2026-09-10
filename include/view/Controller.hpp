#pragma once
#include "view/OrbitCamera.hpp"
#include "render/PickingFrameBuffer.hpp"
#include <GLFW/glfw3.h>
#include <memory>
#include <optional>

struct Inputstate {
    double mouseX = 0.0;
    double mouseY = 0.0;

    double previousMouseX = 0.0;
    double previousMouseY = 0.0;

    double scrollDelta = 0.0;

    bool leftPressed = false;
    bool leftButton = false;
    bool middleButton = false;
    bool rightButton = false;
    bool shift = false;
};

struct mouseCoord{
    int x;
    int y;
};

class Controller{
public:
    Controller();
    ~Controller();

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    bool update(bool allowCameraInput = true);
    GLFWwindow* GetWindow();
    OrbitCamera* GetCamera();
    PickingFrameBuffer* GetPickFramebuffer();
    void GetScreenSize(int& width, int& height);

    void InitPickframebuffer();
    bool consumeLeftPress(std::optional<mouseCoord>& pos);

private:
    GLFWwindow* m_window;
    OrbitCamera* m_camera;
    Inputstate m_input;
    PickingFrameBuffer* m_pickBuffer;
};