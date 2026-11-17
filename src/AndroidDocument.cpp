#include "AndroidDocument.h"

#include "AndroidEncoder.h"

#include <QtGlobal>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QObject>

#include <memory>

// Qt 6.6 publishes the document-picker result through this private helper.
// The versioned private include is added by CMake for the Android target only.
#include <QtCore/private/qandroidextras_p.h>

namespace {
constexpr int kCreateDocumentRequest = 0x5701;
constexpr int kResultOk = -1;

struct DocumentPick {
    bool accepted = false;
    QString uri;
};

// The picker grants this object. Parsing the URI string again can drop that grant.
QJniObject grantedUri;

// ================================================================================

QJniObject androidActivity() {
    return QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity",
                                              "()Landroid/app/Activity;");
}

// ================================================================================

void deleteCreatedDocument(const QJniObject& uri) {
    QJniObject activity = androidActivity();
    if (activity.isValid() == false || uri.isValid() == false) {
        return;
    }
    QJniObject resolver = activity.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    QJniObject::callStaticMethod<jboolean>("android/provider/DocumentsContract", "deleteDocument",
                                           "(Landroid/content/ContentResolver;Landroid/net/Uri;)Z", resolver.object(),
                                           uri.object());
    QJniEnvironment env;
    env->ExceptionClear();
}

// ================================================================================

bool writeStream(const QJniObject& stream, const QString& sourcePath, QString& error) {
    QFile input(sourcePath);
    if (input.open(QIODevice::ReadOnly) == false) {
        error = input.errorString();
        return false;
    }
    const QString failed = QObject::tr("The recording could not be saved to that location.");
    QJniEnvironment env;
    char chunk[65536];
    while (true) {
        const qint64 count = input.read(chunk, sizeof chunk);
        if (count < 0) {
            error = input.errorString();
            return false;
        }
        if (count == 0) {
            break;
        }
        jbyteArray bytes = env->NewByteArray(static_cast<jsize>(count));
        env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(count), reinterpret_cast<const jbyte*>(chunk));
        stream.callMethod<void>("write", "([B)V", bytes);
        env->DeleteLocalRef(bytes);
        if (androidTakeJavaException(error, failed) == true) {
            return false;
        }
    }
    stream.callMethod<void>("flush", "()V");
    stream.callMethod<void>("close", "()V");
    return androidTakeJavaException(error, failed) == false;
}
}  // namespace

// ================================================================================

bool androidCreateDocument(const QString& mimeType, const QString& title, QString& uri, QString& error) {
    error.clear();
    uri.clear();
    auto pick = std::make_shared<DocumentPick>();
    auto loop = std::make_shared<QEventLoop>();

    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString(QStringLiteral("android.intent.action.CREATE_DOCUMENT")).object<jstring>());
    intent.callObjectMethod("addCategory", "(Ljava/lang/String;)Landroid/content/Intent;",
                            QJniObject::fromString(QStringLiteral("android.intent.category.OPENABLE")).object<jstring>());
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                            QJniObject::fromString(mimeType).object<jstring>());
    intent.callObjectMethod("putExtra", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                            QJniObject::fromString(QStringLiteral("android.intent.extra.TITLE")).object<jstring>(),
                            QJniObject::fromString(title).object<jstring>());
    if (androidTakeJavaException(error, QObject::tr("The save dialog could not be opened.")) == true) {
        return false;
    }

    QtAndroidPrivate::startActivity(intent, kCreateDocumentRequest,
                                    [pick, loop](int, int code, const QJniObject& data) {
                                        QString chosen;
                                        QJniObject parsed;
                                        if (code == kResultOk && data.isValid() == true) {
                                            parsed = data.callObjectMethod("getData", "()Landroid/net/Uri;");
                                            if (parsed.isValid() == true) {
                                                chosen = parsed.callObjectMethod("toString", "()Ljava/lang/String;").toString();
                                            }
                                        }
                                        QMetaObject::invokeMethod(
                                                qApp,
                                                [pick, loop, chosen, parsed] {
                                                    grantedUri = parsed;
                                                    pick->uri = chosen;
                                                    pick->accepted = chosen.isEmpty() == false;
                                                    loop->quit();
                                                },
                                                Qt::QueuedConnection);
                                    });
    if (androidTakeJavaException(error, QObject::tr("The save dialog could not be opened.")) == true) {
        return false;
    }
    loop->exec();
    if (pick->accepted == false) {
        error.clear();
        return false;
    }
    uri = pick->uri;
    return uri.isEmpty() == false;
}

// ================================================================================

bool androidCopyFileToContentUri(const QString& uri, const QString& sourcePath, QString& error) {
    error.clear();
    const QString failed = QObject::tr("The recording could not be saved to that location.");
    QJniObject activity = androidActivity();
    if (activity.isValid() == false) {
        error = failed;
        return false;
    }
    QJniObject parsed;
    if (grantedUri.isValid() == true &&
        grantedUri.callObjectMethod("toString", "()Ljava/lang/String;").toString() == uri) {
        parsed = grantedUri;
    } else {
        parsed = QJniObject::callStaticObjectMethod("android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
                                                    QJniObject::fromString(uri).object<jstring>());
    }
    QJniObject resolver = activity.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    QJniObject stream = resolver.callObjectMethod("openOutputStream", "(Landroid/net/Uri;)Ljava/io/OutputStream;",
                                                  parsed.object());
    if (androidTakeJavaException(error, failed) == true || stream.isValid() == false) {
        if (error.isEmpty() == true) {
            error = failed;
        }
        deleteCreatedDocument(parsed);
        return false;
    }
    if (writeStream(stream, sourcePath, error) == false) {
        stream.callMethod<void>("close", "()V");
        QJniEnvironment env;
        env->ExceptionClear();
        deleteCreatedDocument(parsed);
        if (error.isEmpty() == true) {
            error = failed;
        }
        return false;
    }
    return true;
}
