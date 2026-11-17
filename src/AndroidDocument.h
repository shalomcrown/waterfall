#pragma once

#include <QString>

// ACTION_CREATE_DOCUMENT. A cancelled picker returns false and leaves error empty.
bool androidCreateDocument(const QString& mimeType, const QString& title, QString& uri, QString& error);

// Writes sourcePath into a content URI that androidCreateDocument granted.
bool androidCopyFileToContentUri(const QString& uri, const QString& sourcePath, QString& error);
