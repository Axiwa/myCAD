#pragma once
#include <glm/glm.hpp>

struct DraftAnalysisSettings{
    bool enabled = false;
    float pullDirection[3] = { 0.0f, 0.0f, 1.0f };
    float requiredAngleDegrees = 0;
};