#include  "scene/Transform.hpp"

void Transform::setLocalMatrix(const glm::dmat4& matrix){
    local_ = matrix;
    dirty_ = true;
}

const glm::dmat4& Transform::localMatrix() const{
    return local_;
}

const glm::dmat4& Transform::worldMatrix() const{
    return world_;
}

void Transform::updateWorld(const glm::dmat4& parentWorld){
    world_ = parentWorld * local_;
    dirty_ = false;
}

void Transform::updateAsRoot(){
    world_ = local_;
    dirty_ = false;
}

bool Transform::isDirty() const{
    return dirty_;
}