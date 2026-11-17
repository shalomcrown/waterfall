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
}

// ================================================================================

void PcmHistory::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.clear();
    m_data.shrink_to_fit();
    m_next = 0;
    m_count = 0;
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
    }
}

// ================================================================================

int PcmHistory::sampleRate() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sampleRate;
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
