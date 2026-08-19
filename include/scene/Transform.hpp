#pragma once
#include <glm/common.hpp>
#include <glm/matrix.hpp>

class Transform {
public:
    void setLocalMatrix(const glm::dmat4& matrix);

    const glm::dmat4& localMatrix() const;
    const glm::dmat4& worldMatrix() const;

    void updateWorld(const glm::dmat4& parentWorld);
    void updateAsRoot();
    bool isDirty() const;

    void rotateAroundLocal(const glm::dvec3& axis, float rad);
    void rotateAround(const glm::dvec3& axis, float rad);
    void translate(const glm::dvec3& vec);

private:
    glm::dmat4 local_{1.0};
    glm::dmat4 world_{1.0};
    bool dirty_ = true;
};