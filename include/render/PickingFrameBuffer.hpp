#pragma once

#include <glad/glad.h> 
#include <cstdint>

using PickId = std::uint32_t;

class PickingFrameBuffer{
public:
    PickingFrameBuffer() {}
    ~PickingFrameBuffer();
    //PickingFrameBuffer(PickingFrameBuffer&&) noexcept;
    //PickingFrameBuffer& operator=(PickingFrameBuffer&&) noexcept;
    //PickingFrameBuffer(const PickingFrameBuffer&) = delete;
    //PickingFrameBuffer& operator=(const PickingFrameBuffer&) = delete;
    bool resize(int width, int height);
    bool bind();
    void unbind(GLint origFB);
    uint32_t pickID(int x, int y, int width, int height);
    void clear();
    PickId getID() { return id; }
    void setID(PickId in) { id = in; }

private:
    GLuint pickFrameBuffer = 0;
    GLuint colorAttachment = 0;
    GLuint depthAttachment = 0;
    int m_width = 0;
    int m_height = 0;

    PickId id = 0;
};