#pragma once
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include "common/common.h"
#include <vector>

struct Bbox
{
    glm::vec3 minimum;
    glm::vec3 maximum;
    
    Bbox() {
        minimum = glm::vec3(std::numeric_limits<float>::max());
        maximum = glm::vec3(std::numeric_limits<float>::lowest());
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
        maximum = glm::vec3(std::numeric_limits<float>::lowest());
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

    void applyTransform(const glm::mat4& transform){
        if (!valid()){
            return;
        }
        glm::vec3 two[2] = {minimum, maximum};
        reset();
        for(uint8_t i = 0; i < 8; i++){
            uint8_t x = i & 1; // 0 / 1
            uint8_t y = (i & 2) >> 1; // 0 / 1
            uint8_t z = (i & 4) >> 2; // 0 / 1
            auto point = glm::vec3(two[x].x, two[y].y, two[z].z);
            glm::vec3 pos = transform * glm::vec4(point, 1);
            expand(pos);
        }
    }
};
