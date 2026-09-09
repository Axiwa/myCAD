#include "render/PickingFrameBuffer.hpp"
#include  <iostream>

PickingFrameBuffer::~PickingFrameBuffer(){
    clear();
}

void PickingFrameBuffer::clear(){
    if (pickFrameBuffer) {
        glDeleteFramebuffers(1, &pickFrameBuffer);
        pickFrameBuffer = 0;
    }
    if (colorAttachment) {
        glDeleteTextures(1, &colorAttachment);
        colorAttachment = 0;
    }
    if (depthAttachment) {
        glDeleteRenderbuffers(1, &depthAttachment);
        depthAttachment = 0;
    }
}

bool PickingFrameBuffer::resize(int width, int height){
    if (width == 0 || height == 0){
        std::cerr << "Frame buffer size if zero!" << std::endl;
        return false;
    }
    if (m_width == width && m_height == height && pickFrameBuffer){
        return true;
    }

    if (pickFrameBuffer) {
        glDeleteFramebuffers(1, &pickFrameBuffer);
        pickFrameBuffer = 0;
    }
    if (colorAttachment) {
        glDeleteTextures(1, &colorAttachment);
        colorAttachment = 0;
    }
    if (depthAttachment) {
        glDeleteRenderbuffers(1, &depthAttachment);
        depthAttachment = 0;
    }

    glGenFramebuffers(1, &pickFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, pickFrameBuffer);
     
        glGenTextures(1, &colorAttachment);
        glBindTexture(GL_TEXTURE_2D, colorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorAttachment, 0);

        glGenRenderbuffers(1, &depthAttachment);
        glBindRenderbuffer(GL_RENDERBUFFER, depthAttachment);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthAttachment);

        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		std::cerr << "Frame buffer is not configured!" << std::endl;
        std::cout << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        clear();
		return false;
	}

    m_width = width;
    m_height = height;

    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    return true;
}

uint32_t PickingFrameBuffer::pickID(int pixelX, int pixelY){
    if (pixelX < 0 || pixelY < 0 || pixelX >= m_width || pixelY >= m_height) {
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, pickFrameBuffer);  // 绑定你的 FBO
    glReadBuffer(GL_COLOR_ATTACHMENT0);                  // 指定要读取的 color attachment

    uint32_t pixelID;
    glReadPixels(pixelX, pixelY, 1, 1,                   // 位置和大小
        GL_RED_INTEGER,                         // 格式（与纹理格式匹配）
        GL_UNSIGNED_INT,                        // 类型（与纹理类型匹配）
        &pixelID);                              // 输出数据

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return pixelID;
}

bool PickingFrameBuffer::bind(){
    if (pickFrameBuffer){
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pickFrameBuffer);
		glViewport(0, 0, m_width, m_height);

        GLuint clearValue = 0;
        glClearBufferuiv(GL_COLOR, 0, &clearValue);
        
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        return true;
    }
    std::cerr<<"Invalid picking framebuffer"<<std::endl;
    return false;
}

void PickingFrameBuffer::unbind(GLint origFB){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, origFB);
}