#pragma once
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include "common/common.h"

struct Bbox
{
    glm::vec3 minimum;
    glm::vec3 maximum;

    Bbox() {
        minimum = glm::vec3(std::numeric_limits<float>::max());
        maximum = glm::vec3(std::numeric_limits<float>::min());
    }

    glm::vec3 center() const{
        return (minimum + maximum) * 0.5f;
    }

    float radius() const{
        float value = glm::length(maximum - minimum) * 0.5f;
        return std::max(value, 0.001f);
    }

    void reset() {
        minimum = glm::vec3(std::numeric_limits<float>::max());
        maximum = glm::vec3(std::numeric_limits<float>::min());
    }

    void expand(const glm::vec3& point){
        minimum = glm::min(point, minimum);
        maximum = glm::max(point, maximum);
    }

    bool valid() const{ // TODO: collapsed to point also OK now
        return (
            minimum.x <= maximum.x && minimum.y < maximum.y + EPSILONF && minimum.z < maximum.z + EPSILONF
         || minimum.x < maximum.x + EPSILONF && minimum.y <= maximum.y && minimum.z < maximum.z + EPSILONF
         || minimum.x < maximum.x && minimum.y < maximum.y && minimum.z < maximum.z + EPSILONF
        );
    }
};
