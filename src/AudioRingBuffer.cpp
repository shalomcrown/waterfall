#include "AudioRingBuffer.h"

#include <algorithm>

namespace {
size_t roundUpPowerOfTwo(size_t value) {
    size_t capacity = 1;
    while (capacity < value) {
        capacity <<= 1;
    }
    if (capacity < 2) {
        capacity = 2;
    }
    return capacity;
}
}  // namespace

// ================================================================================

AudioRingBuffer::AudioRingBuffer(size_t capacity)
        : m_data(roundUpPowerOfTwo(capacity), 0.f), m_mask(m_data.size() - 1) {}

// ================================================================================

size_t AudioRingBuffer::write(std::span<const float> samples) {
    const size_t head = m_head.load(std::memory_order_relaxed);
    const size_t tail = m_tail.load(std::memory_order_acquire);
    const size_t freeSamples = m_data.size() - (head - tail);
    const size_t count = std::min(samples.size(), freeSamples);
    for (size_t i = 0; i < count; ++i) {
        m_data[(head + i) & m_mask] = samples[i];
    }
    m_head.store(head + count, std::memory_order_release);
    return count;
}

// ================================================================================

size_t AudioRingBuffer::read(std::span<float> destination) {
    const size_t tail = m_tail.load(std::memory_order_relaxed);
    const size_t head = m_head.load(std::memory_order_acquire);
    const size_t available = head - tail;
    const size_t count = std::min(destination.size(), available);
    for (size_t i = 0; i < count; ++i) {
        destination[i] = m_data[(tail + i) & m_mask];
    }
    m_tail.store(tail + count, std::memory_order_release);
    return count;
}

// ================================================================================

size_t AudioRingBuffer::availableForRead() const {
    const size_t tail = m_tail.load(std::memory_order_relaxed);
    const size_t head = m_head.load(std::memory_order_acquire);
    return head - tail;
}

// ================================================================================

size_t AudioRingBuffer::availableForWrite() const {
    const size_t head = m_head.load(std::memory_order_relaxed);
    const size_t tail = m_tail.load(std::memory_order_acquire);
    return m_data.size() - (head - tail);
}
