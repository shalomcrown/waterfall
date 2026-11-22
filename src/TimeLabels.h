#pragma once

#include <QString>

#include <algorithm>
#include <cmath>

// Time axis text. A live session shows the clock, a loaded file shows the
// position in the file.

// The smallest tidy step that is at least target seconds.
inline double niceTimeStep(double target) {
    static const double steps[] = {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0,
                                   2.0,   5.0,   10.0,  15.0, 30.0, 60.0, 120.0, 300.0, 600.0, 900.0,
                                   1800.0, 3600.0, 7200.0, 10800.0, 21600.0, 43200.0, 86400.0};
    for (double step : steps) {
        if (step >= target) {
            return step;
        }
    }
    return steps[sizeof(steps) / sizeof(steps[0]) - 1];
}

// ================================================================================

// localMs is milliseconds since the epoch with the local UTC offset already added.
inline QString formatClockTime(long long localMs, double stepSeconds) {
    constexpr long long kDay = 86400000LL;
    const long long ofDay = ((localMs % kDay) + kDay) % kDay;
    const int hours = static_cast<int>(ofDay / 3600000LL);
    const int minutes = static_cast<int>((ofDay / 60000LL) % 60);
    const int seconds = static_cast<int>((ofDay / 1000LL) % 60);
    const int millis = static_cast<int>(ofDay % 1000LL);
    QString text = QStringLiteral("%1:%2").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0'));
    if (stepSeconds >= 60.0) {
        return text;
    }
    text += QStringLiteral(":%1").arg(seconds, 2, 10, QLatin1Char('0'));
    if (stepSeconds >= 1.0) {
        return text;
    }
    if (stepSeconds >= 0.1) {
        return text + QStringLiteral(".%1").arg(millis / 100);
    }
    return text + QStringLiteral(".%1").arg(millis, 3, 10, QLatin1Char('0'));
}

// ================================================================================

inline QString formatPosition(double seconds, double stepSeconds) {
    const long long totalMs = std::llround(std::max(0.0, seconds) * 1000.0);
    const long long totalSeconds = totalMs / 1000;
    const int hours = static_cast<int>(totalSeconds / 3600);
    const int minutes = static_cast<int>((totalSeconds / 60) % 60);
    const int secs = static_cast<int>(totalSeconds % 60);
    const int millis = static_cast<int>(totalMs % 1000);
    QString text;
    if (hours > 0) {
        text = QStringLiteral("%1:%2:%3")
                       .arg(hours)
                       .arg(minutes, 2, 10, QLatin1Char('0'))
                       .arg(secs, 2, 10, QLatin1Char('0'));
    } else {
        text = QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'));
    }
    if (stepSeconds >= 1.0) {
        return text;
    }
    if (stepSeconds >= 0.1) {
        return text + QStringLiteral(".%1").arg(millis / 100);
    }
    return text + QStringLiteral(".%1").arg(millis, 3, 10, QLatin1Char('0'));
}
