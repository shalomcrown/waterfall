#include "ScreenAwake.h"

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

#if defined(WATERFALL_HAS_DBUS)
#include <QDBusInterface>
#include <QDBusReply>
#include <QString>
#endif

namespace {
#if defined(WATERFALL_HAS_DBUS)
uint g_cookie = 0;
#endif

void releaseInhibit() {
#if defined(WATERFALL_HAS_DBUS)
    if (g_cookie == 0) {
        return;
    }
    QDBusInterface saver(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                         QStringLiteral("org.freedesktop.ScreenSaver"));
    saver.call(QStringLiteral("UnInhibit"), g_cookie);
    g_cookie = 0;
#endif
}

// ================================================================================

void applyAndroidKeepScreen(bool keep) {
#if defined(Q_OS_ANDROID)
    // Window flags must be changed on the Android UI thread. The Qt thread is not that thread.
    // QtNative.activity() is the real Activity. QAndroidApplication::context() is not always one.
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([keep] {
        QJniObject activity = QJniObject::callStaticObjectMethod(
                "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
        if (activity.isValid() == false) {
            return;
        }
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (window.isValid() == false) {
            return;
        }
        const jint flag = 128;  // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        if (keep == true) {
            window.callMethod<void>("addFlags", "(I)V", flag);
        } else {
            window.callMethod<void>("clearFlags", "(I)V", flag);
        }
        const jboolean on = static_cast<jboolean>(keep ? 1 : 0);
        QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
        if (decor.isValid() == true) {
            decor.callMethod<void>("setKeepScreenOn", "(Z)V", on);
        }
        // android.R.id.content
        QJniObject content = activity.callObjectMethod("findViewById", "(I)Landroid/view/View;", 0x01020002);
        if (content.isValid() == true) {
            content.callMethod<void>("setKeepScreenOn", "(Z)V", on);
        }
        QJniEnvironment env;
        env.checkAndClearExceptions();
    });
#else
    (void)keep;
#endif
}
}  // namespace

// ================================================================================

void ScreenAwake::setKeepAwake(bool keep) {
#if defined(Q_OS_ANDROID)
    applyAndroidKeepScreen(keep);
#elif defined(WATERFALL_HAS_DBUS)
    if (keep == false) {
        releaseInhibit();
        return;
    }
    if (g_cookie != 0) {
        return;
    }
    QDBusInterface saver(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                         QStringLiteral("org.freedesktop.ScreenSaver"));
    const QDBusReply<uint> reply =
            saver.call(QStringLiteral("Inhibit"), QStringLiteral("waterfall"), QStringLiteral("Waterfall is running"));
    if (reply.isValid() == true && reply.value() != 0) {
        g_cookie = reply.value();
    }
#else
    (void)keep;
#endif
}
