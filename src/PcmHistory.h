#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

// The most recent mono samples, stored as 16-bit PCM. Older samples fall off
// the front once the window is full.
class PcmHistory {
public:
    void reset(int sampleRate, int seconds);
    void clear();
    void append(std::span<const float> samples);

    int sampleRate() const;
    std::vector<std::int16_t> samples() const;

private:
    mutable std::mutex m_mutex;
    std::vector<std::int16_t> m_data;
    std::size_t m_next = 0;
    std::size_t m_count = 0;
    int m_sampleRate = 0;
};
