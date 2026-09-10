#pragma once
#include <cstdint>
#include <chrono>

struct RenderStats {
    size_t uniquePrototypeTriangles = 0;
    size_t visibleOccurrences = 0;
    size_t submittedTriangles = 0;
    size_t drawCalls = 0;
    size_t culledOccurrences = 0;
    double cpuRenderMilliseconds = 0.0;
};

class Profiler {
public:
    void start() {
        m_start = std::chrono::steady_clock::now();
    }
    void accumulate() {
        auto m_current = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(m_current - m_start);

        m_totalDuration += elapsed.count() / 1000.0; // ms
        m_frameCount++;
    }
    uint32_t getTotalFrames() const{
        return m_frameCount;
    }
    double getAvgTime(){
        if (m_frameCount == 0) return 0;
        double avg = m_totalDuration / m_frameCount;
        m_totalDuration = 0;
        m_frameCount = 0;
        return avg;
    }
    uint32_t warmupCount = 120;
    uint32_t maxCount = 120;
private:
    std::chrono::steady_clock::time_point m_start;
    uint32_t m_frameCount = 0;
    double m_totalDuration = 0;
};