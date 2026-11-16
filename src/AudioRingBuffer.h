#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

// Single-producer, single-consumer float queue. The producer and the consumer
// must each stay on one thread. Indexes are monotonic, so the buffer can fill
// completely. A full write drops the samples that do not fit.
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacity);

    size_t capacity() const { return m_data.size(); }
    size_t write(std::span<const float> samples);
    size_t read(std::span<float> destination);
    size_t availableForRead() const;
    size_t availableForWrite() const;

private:
    std::vector<float> m_data;
    size_t m_mask = 0;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
};
