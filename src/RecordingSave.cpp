#include "RecordingSave.h"

#include "AppSettings.h"
#include "WavFile.h"

#include <QtGlobal>

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QStyle>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#if defined(Q_OS_ANDROID)
#include "AndroidDocument.h"
#include "AndroidEncoder.h"
#endif

namespace {
RecordingFormat makeFormat(const QString& id, const QString& label, const QString& extension, const QString& mimeType) {
    RecordingFormat format;
    format.id = id;
    format.label = label;
    format.extension = extension;
    format.mimeType = mimeType;
    return format;
}

// ================================================================================

RecordingFormat wavFormat() {
    return makeFormat(QStringLiteral("wav"), QObject::tr("WAV (uncompressed)"), QStringLiteral("wav"),
                      QStringLiteral("audio/wav"));
}

// ================================================================================

QList<RecordingFormat> discoverFormats() {
    QList<RecordingFormat> found;
    found.append(wavFormat());
#if defined(Q_OS_ANDROID)
    if (androidEncoderAvailable(QStringLiteral("audio/mp4a-latm")) == true) {
        found.append(makeFormat(QStringLiteral("m4a"), QObject::tr("M4A (AAC)"), QStringLiteral("m4a"),
                                QStringLiteral("audio/mp4")));
    }
    if (androidSdkInt() >= 29 && androidEncoderAvailable(QStringLiteral("audio/opus")) == true) {
        found.append(makeFormat(QStringLiteral("ogg"), QObject::tr("OGG (Opus)"), QStringLiteral("ogg"),
                                QStringLiteral("audio/ogg")));
    }
#endif
    return found;
}

// ================================================================================

bool formatIsKnown(const RecordingFormat& format) {
    for (const RecordingFormat& item : RecordingSave::formats()) {
        if (item.id == format.id) {
            return true;
        }
    }
    return false;
}

// ================================================================================

bool writeFilesystem(const QString& path, const RecordingFormat& format, int sampleRate,
                     std::span<const std::int16_t> samples, QString& error) {
    const QFileInfo info(path);
    if (info.absolutePath().isEmpty() == false && QDir().mkpath(info.absolutePath()) == false) {
        error = QObject::tr("The folder could not be created.");
        return false;
    }
    if (format.id == QStringLiteral("wav")) {
        return WavFile::write(path, sampleRate, samples, error);
    }
#if defined(Q_OS_ANDROID)
    return androidEncodeFile(path, format.id, sampleRate, samples, error);
#else
    error = QObject::tr("This format is not available.");
    return false;
#endif
}

#if defined(Q_OS_ANDROID)

// ================================================================================

struct RemoveFile {
    QString path;
    ~RemoveFile() {
        if (path.isEmpty() == false) {
            QFile::remove(path);
        }
    }
};

// ================================================================================

QString temporaryRecordingPath(const RecordingFormat& format) {
    QString folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (folder.isEmpty() == true) {
        folder = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (folder.isEmpty() == true) {
        return {};
    }
    QDir().mkpath(folder);
    return QDir(folder).filePath(QStringLiteral("waterfall-%1.%2")
                                         .arg(QUuid::createUuid().toString(QUuid::WithoutBraces), format.extension));
}

// ================================================================================

bool writeContent(const QString& destination, const RecordingFormat& format, int sampleRate,
                  std::span<const std::int16_t> samples, QString& error) {
    const QString temp = temporaryRecordingPath(format);
    if (temp.isEmpty() == true) {
        error = QObject::tr("The recording could not be saved.");
        return false;
    }
    const RemoveFile cleanup{temp};
    if (writeFilesystem(temp, format, sampleRate, samples, error) == false) {
        return false;
    }
    return androidCopyFileToContentUri(destination, temp, error);
}

// ================================================================================

bool chooseFormat(QWidget* parent, RecordingFormat& chosen) {
    const QList<RecordingFormat> formats = RecordingSave::formats();
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Save recording"));
    dialog.setModal(true);
    dialog.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    dialog.setWindowState(Qt::WindowFullScreen);
    if (parent != nullptr && parent->screen() != nullptr) {
        QScreen* screen = parent->screen();
        dialog.setGeometry(screen->geometry());
        QObject::connect(screen, &QScreen::geometryChanged, &dialog, [&dialog](const QRect& geometry) {
            dialog.setGeometry(geometry);
            dialog.setWindowState(dialog.windowState() | Qt::WindowFullScreen);
        });
    }

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* header = new QHBoxLayout();
    auto* back = new QPushButton(dialog.style()->standardIcon(QStyle::SP_ArrowBack), QObject::tr("Back"), &dialog);
    back->setObjectName(QStringLiteral("cancelSave"));
    back->setAccessibleName(QObject::tr("Back"));
    back->setMinimumHeight(44);
    auto* title = new QLabel(QObject::tr("Save recording"), &dialog);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    header->addWidget(back);
    header->addWidget(title, 1);
    layout->addLayout(header);

    auto* combo = new QComboBox(&dialog);
    combo->setObjectName(QStringLiteral("saveFormat"));
    combo->setAccessibleName(QObject::tr("Format"));
    combo->setMinimumHeight(44);
    for (const RecordingFormat& format : formats) {
        combo->addItem(format.label, format.id);
    }
    const int saved = combo->findData(AppSettings::recordingFormat());
    combo->setCurrentIndex(saved < 0 ? 0 : saved);
    auto* form = new QFormLayout();
    form->addRow(QObject::tr("Format"), combo);
    layout->addLayout(form);

    auto* save = new QPushButton(QObject::tr("Save"), &dialog);
    save->setObjectName(QStringLiteral("acceptSave"));
    save->setMinimumHeight(44);
    layout->addWidget(save);
    layout->addStretch(1);
    QObject::connect(back, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() == QDialog::Rejected) {
        return false;
    }
    chosen = RecordingSave::formatById(combo->currentData().toString());
    AppSettings::setRecordingFormat(chosen.id);
    return true;
}
#else
// ================================================================================

QString withExtension(const QString& path, const QString& extension) {
    const QFileInfo info(path);
    if (info.suffix().compare(extension, Qt::CaseInsensitive) == 0) {
        return path;
    }
    const QString base = info.completeBaseName().isEmpty() ? info.fileName() : info.completeBaseName();
    return QDir(info.path()).filePath(base + QStringLiteral(".") + extension);
}

// ================================================================================

bool promptDesktop(QWidget* parent, int sampleRate, std::span<const std::int16_t> samples, QString& error) {
    QString directory = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (directory.isEmpty() == true) {
        directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (directory.isEmpty() == false) {
        QDir().mkpath(directory);
    }

    const QList<RecordingFormat> formats = RecordingSave::formats();
    QFileDialog dialog(parent, QObject::tr("Save recording"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    QStringList filters;
    QString selectedFilter;
    const QString savedId = AppSettings::recordingFormat();
    for (const RecordingFormat& format : formats) {
        const QString filter = QObject::tr("%1 (*.%2)").arg(format.label, format.extension);
        filters.append(filter);
        if (format.id == savedId) {
            selectedFilter = filter;
        }
    }
    dialog.setNameFilters(filters);
    if (selectedFilter.isEmpty() == false) {
        dialog.selectNameFilter(selectedFilter);
    }
    if (directory.isEmpty() == false) {
        dialog.setDirectory(directory);
    }
    const RecordingFormat initial = RecordingSave::formatById(savedId);
    dialog.selectFile(RecordingSave::fileName(initial, QDateTime::currentDateTime()));
    if (dialog.exec() == QDialog::Rejected) {
        return false;
    }
    const QStringList files = dialog.selectedFiles();
    if (files.isEmpty() == true) {
        return false;
    }

    RecordingFormat chosen = formats.first();
    const QString chosenFilter = dialog.selectedNameFilter();
    for (const RecordingFormat& format : formats) {
        const QString filter = QObject::tr("%1 (*.%2)").arg(format.label, format.extension);
        if (filter == chosenFilter) {
            chosen = format;
        }
    }
    AppSettings::setRecordingFormat(chosen.id);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool wrote =
            RecordingSave::write(withExtension(files.first(), chosen.extension), chosen, sampleRate, samples, error);
    QApplication::restoreOverrideCursor();
    return wrote;
}
#endif
}  // namespace

// ================================================================================

QList<RecordingFormat> RecordingSave::formats() {
    static const QList<RecordingFormat> cached = discoverFormats();
    return cached;
}

// ================================================================================

RecordingFormat RecordingSave::formatById(const QString& id) {
    for (const RecordingFormat& format : formats()) {
        if (format.id == id) {
            return format;
        }
    }
    return wavFormat();
}

// ================================================================================

bool RecordingSave::isContentUri(const QString& destination) {
    return destination.startsWith(QStringLiteral("content:"), Qt::CaseInsensitive);
}

// ================================================================================

QString RecordingSave::fileName(const RecordingFormat& format, const QDateTime& when) {
    return QStringLiteral("waterfall-%1.%2").arg(when.toString(QStringLiteral("yyyyMMdd-HHmmss")), format.extension);
}

// ================================================================================

bool RecordingSave::write(const QString& destination, const RecordingFormat& format, int sampleRate,
                          std::span<const std::int16_t> samples, QString& error) {
    error.clear();
    if (sampleRate < 1 || samples.empty() == true) {
        error = QObject::tr("There is no audio to save.");
        return false;
    }
    if (formatIsKnown(format) == false) {
        error = QObject::tr("This format is not available.");
        return false;
    }
    // QFile cannot write a content URI. The document picker grants the URI it returns,
    // and the bytes have to go through that grant.
    if (isContentUri(destination) == true) {
#if defined(Q_OS_ANDROID)
        return writeContent(destination, format, sampleRate, samples, error);
#else
        error = QObject::tr("That location cannot be written. Choose a folder the app is allowed to use.");
        return false;
#endif
    }
    return writeFilesystem(destination, format, sampleRate, samples, error);
}

// ================================================================================

bool RecordingSave::promptAndWrite(QWidget* parent, int sampleRate, std::span<const std::int16_t> samples, QString& error) {
    error.clear();
    if (sampleRate < 1 || samples.empty() == true) {
        error = QObject::tr("There is no audio to save.");
        return false;
    }
#if defined(Q_OS_ANDROID)
    RecordingFormat format;
    if (chooseFormat(parent, format) == false) {
        return false;
    }
    QString uri;
    if (androidCreateDocument(format.mimeType, fileName(format, QDateTime::currentDateTime()), uri, error) == false) {
        return false;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool wrote = write(uri, format, sampleRate, samples, error);
    QApplication::restoreOverrideCursor();
    return wrote;
#else
    return promptDesktop(parent, sampleRate, samples, error);
#endif
}
