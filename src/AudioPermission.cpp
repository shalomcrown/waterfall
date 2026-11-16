#include "AudioPermission.h"

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QCoreApplication>
#include <QPermissions>
#endif

#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>
#endif

AudioPermission::Status AudioPermission::status() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QMicrophonePermission microphone;
    switch (qApp->checkPermission(microphone)) {
    case Qt::PermissionStatus::Granted:
        return Status::Granted;
    case Qt::PermissionStatus::Denied:
        return Status::Denied;
    case Qt::PermissionStatus::Undetermined:
        return Status::Undetermined;
    }
    return Status::Undetermined;
#else
    // Qt 6.4 has no runtime permission API. Desktop builds treat the microphone
    // as available and only Android (Qt 6.6) asks the system.
    return Status::Granted;
#endif
}

// ================================================================================

void AudioPermission::request(const std::function<void(Status)>& done) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QMicrophonePermission microphone;
    qApp->requestPermission(microphone, qApp, [done](const QPermission& permission) {
        const Status result = permission.status() == Qt::PermissionStatus::Granted ? Status::Granted
                              : permission.status() == Qt::PermissionStatus::Denied ? Status::Denied
                                                                                     : Status::Undetermined;
        done(result);
    });
#else
    done(Status::Granted);
#endif
}

// ================================================================================

bool AudioPermission::canOpenSystemSettings() {
#if defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

// ================================================================================

bool AudioPermission::openSystemSettings() {
#if defined(Q_OS_ANDROID)
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (activity.isValid() == false) {
        return false;
    }
    const QString package =
            activity.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString(QStringLiteral("android.settings.APPLICATION_DETAILS_SETTINGS"))
                              .object<jstring>());
    QJniObject uri = QJniObject::callStaticObjectMethod(
            "android/net/Uri", "fromParts",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Landroid/net/Uri;",
            QJniObject::fromString(QStringLiteral("package")).object<jstring>(),
            QJniObject::fromString(package).object<jstring>(), nullptr);
    intent.callObjectMethod("setData", "(Landroid/net/Uri;)Landroid/content/Intent;", uri.object());
    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
    return true;
#else
    return false;
#endif
}

// ================================================================================

QString AudioPermission::instructions() {
#if defined(Q_OS_ANDROID)
    return QStringLiteral(
            "Waterfall needs the microphone to read the input device. "
            "Open the app settings and allow Microphone, then come back.");
#else
    return QStringLiteral(
            "Waterfall needs the microphone to read the input device. "
            "Allow microphone access for this app in the system privacy settings, then reopen Settings.");
#endif
}
