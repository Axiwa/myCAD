#pragma once

#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <cstdint>

using PickId = std::uint32_t;

class PickingFrameBuffer{
public:
    PickingFrameBuffer() {}
    ~PickingFrameBuffer();
    //PickingFrameBuffer(PickingFrameBuffer&&) noexcept;
    //PickingFrameBuffer& operator=(PickingFrameBuffer&&) noexcept;
    PickingFrameBuffer(const PickingFrameBuffer&) = delete;
    PickingFrameBuffer& operator=(const PickingFrameBuffer&) = delete;
    bool resize(int width, int height);
    bool bind();
    void unbind(GLint origFB, GLFWwindow* window);
    uint32_t pickID(int x, int y);
    void clear();

private:
    GLuint pickFrameBuffer = 0;
    GLuint colorAttachment = 0;
    GLuint depthAttachment = 0;
    int m_width = 0;
    int m_height = 0;
};