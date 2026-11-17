#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include <cstdint>
#include <span>

class QWidget;

struct RecordingFormat {
    QString id;
    QString label;
    QString extension;
    QString mimeType;
};

// Formats this process can actually write. WAV is always first.
class RecordingSave {
public:
    static QList<RecordingFormat> formats();
    static RecordingFormat formatById(const QString& id);
    static bool isContentUri(const QString& destination);
    static QString fileName(const RecordingFormat& format, const QDateTime& when);

    // A content URI is not opened with QFile. On Android it uses the document grant.
    static bool write(const QString& destination, const RecordingFormat& format, int sampleRate,
                      std::span<const std::int16_t> samples, QString& error);

    // False with an empty error means the user cancelled.
    static bool promptAndWrite(QWidget* parent, int sampleRate, std::span<const std::int16_t> samples, QString& error);
};
