#include "view/OrbitCamera.hpp"
#include <algorithm>

OrbitCamera::OrbitCamera(){
    m_projection = glm::perspective(fov_, (float)viewWidth_/(float)viewHeight_, near_, far_);
    yaw(glm::radians(45.f));
    pitch(glm::radians(-30.f));
}

OrbitCamera::OrbitCamera(int width, int height, float fov, float near, float far):
    viewWidth_(width),
    viewHeight_(height),
    fov_(fov),
    near_(near),
    far_(far)
{
    m_projection = glm::perspective(fov_, (float)viewWidth_/(float)viewHeight_, near_, far_);
    yaw(glm::radians(45.f));
    pitch(glm::radians(-30.f));
}

glm::vec3 OrbitCamera::position() const{
    const glm::vec3 offset = orientation_ * glm::vec3(0.0, 0.0, distance_);
    return target_ + offset;
}

glm::vec3 OrbitCamera::up() const{
    return orientation_ * glm::vec3(0.0, 1.0, 0.0);
}

glm::mat4 OrbitCamera::viewMatrix() const {
    return glm::lookAt(position(), target_, up());
}

glm::mat4 OrbitCamera::projectionMatrix() {
    if (isProjectionDirty){
        m_projection = glm::perspective(fov_, (float)viewWidth_/(float)viewHeight_, near_, far_);
        isProjectionDirty = false;
    }
    return m_projection;
}

void OrbitCamera::yaw(float angle){
    const glm::quat delta = glm::angleAxis(angle, glm::vec3(0.0, 1.0, 0.0));
    orientation_ = glm::normalize(delta * orientation_);
}

void OrbitCamera::pitch(float angle){
    const glm::quat delta = glm::angleAxis(angle, glm::vec3(1.0, 0.0, 0.0));
    orientation_ = glm::normalize(orientation_ * delta);
}

void OrbitCamera::dolly(float times){
    distance_ *= times;
    distance_ = std::max(distance_, 0.01f);
    isProjectionDirty = true;
}

void OrbitCamera::pan(float dx, float dy){
    glm::vec3 updir = up();
    glm::vec3 rightdir = orientation_ * glm::vec3(1.0f, 0.0f, 0.0f);
    float worldUnit = 2.0f * distance_ * std::tan(fov_ * 0.5f) / viewHeight_;
    target_ += (-rightdir * dx + updir * dy) * worldUnit;
}

void OrbitCamera::orbit(float dx, float dy){
    yaw(dx);
    pitch(dy);
}

void OrbitCamera::setViewport(int width, int height){
    if (width > 0 && height > 0){
        viewWidth_ = width;
        viewHeight_ = height;
        isProjectionDirty = true;
    }
}
void OrbitCamera::setFOV(float fov){
    fov_ = fov;
    isProjectionDirty = true;
}
void OrbitCamera::setNear(float near){
    near_ = near;
    isProjectionDirty = true;
}
void OrbitCamera::setFar(float far){
    far_ = far;
    isProjectionDirty = true;
}
void OrbitCamera::setDistance(float distance){
    distance_ = std::max(0.01f, distance);
    isProjectionDirty = true;
}

void OrbitCamera::GetViewport(int& width, int& height) const {
    width = viewWidth_;
    height = viewHeight_;
}

void OrbitCamera::fitToBbox(const Bbox& bbox){
    if (!bbox.valid()){
        return;
    }
    target_ = bbox.center();

    float radius = bbox.radius();
    float tanfov = std::tan(fov_ / 2);

    if (viewHeight_ > viewWidth_) {
        float aspect = (float)viewWidth_ / viewHeight_;
        tanfov = tanfov * aspect;
    }
    float sinfov = tanfov / std::sqrt(1 / (1 + tanfov * tanfov));
    distance_ = radius / sinfov * 1.2f;

    //far_ = distance_ + 2 * radius;

    isProjectionDirty = true;
}