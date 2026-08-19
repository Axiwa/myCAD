#pragma once
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "view/Bbox.hpp"

class OrbitCamera{
public:
    OrbitCamera();
    OrbitCamera(int width, int height, float fov = 0.7854f, float near = 0.1f, float far = 1000.0f);
    glm::vec3 position() const;
    glm::vec3 up() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix();

    void yaw(float angle);
    void pitch(float angle);
    void dolly(float times);
    void pan(float dx, float dy);
    void orbit(float dx, float dy);

    void setViewport(int width, int height);
    void setFOV(float fov);
    void setNear(float near);
    void setFar(float far);
    void setDistance(float distance);

    void GetViewport(int& width, int& height) const;

    void fitToBbox(const Bbox& bbox);

private:
    glm::vec3 target_{0.0f};
    glm::quat orientation_{1.0f, 0.0f, 0.0f, 0.0f};
    
    float distance_ = 5.0f;

    int viewWidth_ = 1024;
    int viewHeight_ = 1024;
    float fov_ = glm::radians(45.0f);
    float near_ = 0.3f;
    float far_ = 1000.0f;

    glm::mat4 m_projection{1.0f};
    bool isProjectionDirty = false;
};