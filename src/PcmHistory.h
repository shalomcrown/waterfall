#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

// The most recent mono samples, stored as 16-bit PCM. Older samples fall off
// the front once the window is full. Every sample has an absolute position:
// the first sample after reset() is position 0, and positions keep counting
// after old samples are dropped.
class PcmHistory {
public:
    void reset(int sampleRate, int seconds);
    void clear();
    void append(std::span<const float> samples);

    int sampleRate() const;
    std::vector<std::int16_t> samples() const;

    // Absolute position of the oldest kept sample, and one past the newest.
    long long firstPosition() const;
    long long endPosition() const;
    // Fills out with the samples that start at the absolute position. Returns false
    // when any part of that range has been dropped or has not arrived yet.
    bool readRange(long long start, std::span<float> out) const;

private:
    mutable std::mutex m_mutex;
    std::vector<std::int16_t> m_data;
    std::size_t m_next = 0;
    std::size_t m_count = 0;
    long long m_total = 0;
    int m_sampleRate = 0;
};
