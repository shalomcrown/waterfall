#pragma once

#include <QByteArray>
#include <QString>

// ACTION_CREATE_DOCUMENT. A cancelled picker returns false and leaves error empty.
bool androidCreateDocument(const QString& mimeType, const QString& title, QString& uri, QString& error);

// ACTION_OPEN_DOCUMENT for WAV and MP3. A cancelled picker returns false and leaves error empty.
bool androidOpenDocument(QString& uri, QString& error);
bool androidReadDocument(const QString& uri, QByteArray& bytes, QString& error);

// Writes sourcePath into a content URI that androidCreateDocument granted.
bool androidCopyFileToContentUri(const QString& uri, const QString& sourcePath, QString& error);
