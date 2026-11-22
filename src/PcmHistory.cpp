#include "PcmHistory.h"

#include <algorithm>
#include <cmath>

namespace {
std::int16_t toPcm16(float sample) {
    if (std::isfinite(sample) == false) {
        return 0;
    }
    const float clamped = std::clamp(sample, -1.f, 1.f);
    return static_cast<std::int16_t>(std::lround(clamped * 32767.f));
}
}  // namespace

// ================================================================================

void PcmHistory::reset(int sampleRate, int seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sampleRate = sampleRate > 0 ? sampleRate : 0;
    const long long count = m_sampleRate > 0 && seconds > 0 ? static_cast<long long>(m_sampleRate) * seconds : 0;
    m_data.assign(static_cast<std::size_t>(count), 0);
    m_next = 0;
    m_count = 0;
    m_total = 0;
}

// ================================================================================

void PcmHistory::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.clear();
    m_data.shrink_to_fit();
    m_next = 0;
    m_count = 0;
    m_total = 0;
    m_sampleRate = 0;
}

// ================================================================================

void PcmHistory::append(std::span<const float> samples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.empty() == true) {
        return;
    }
    for (float sample : samples) {
        m_data[m_next] = toPcm16(sample);
        m_next += 1;
        if (m_next >= m_data.size()) {
            m_next = 0;
        }
        if (m_count < m_data.size()) {
            m_count += 1;
        }
        m_total += 1;
    }
}

// ================================================================================

int PcmHistory::sampleRate() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sampleRate;
}

// ================================================================================

long long PcmHistory::firstPosition() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_total - static_cast<long long>(m_count);
}

// ================================================================================

long long PcmHistory::endPosition() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_total;
}

// ================================================================================

bool PcmHistory::readRange(long long start, std::span<float> out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.empty() == true || start < 0) {
        return false;
    }
    const long long first = m_total - static_cast<long long>(m_count);
    const long long end = start + static_cast<long long>(out.size());
    if (start < first || end > m_total) {
        return false;
    }
    // m_next is the slot for position m_total, so position p lives at
    // (m_next + size - (m_total - p)) modulo the ring size.
    const long long size = static_cast<long long>(m_data.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        const long long behind = m_total - (start + static_cast<long long>(i));
        const std::size_t slot = static_cast<std::size_t>((static_cast<long long>(m_next) + size - behind) % size);
        // append() scales by 32767, so this restores the exact input.
        out[i] = static_cast<float>(m_data[slot]) / 32767.f;
    }
    return true;
}

// ================================================================================

std::vector<std::int16_t> PcmHistory::samples() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::int16_t> ordered(m_count);
    if (m_count == 0 || m_data.empty() == true) {
        return ordered;
    }
    const std::size_t start = (m_next + m_data.size() - m_count) % m_data.size();
    for (std::size_t i = 0; i < m_count; ++i) {
        ordered[i] = m_data[(start + i) % m_data.size()];
    }
    return ordered;
}
