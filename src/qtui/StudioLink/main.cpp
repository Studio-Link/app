#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QOperatingSystemVersion>
#ifdef SL_BARESIP_BACKEND
#include "mythread.h"
#endif

int main(int argc, char *argv[])
{
    int err = 0;

    //Angle workaround on windows 8 and older
    if (QOperatingSystemVersion::current() <= QOperatingSystemVersion::Windows8)
    {
        qDebug("Windows 7 Workaround");
        qputenv("QT_ANGLE_PLATFORM", "warp");
    }

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);
#ifdef SL_BARESIP_BACKEND
    MyThread thread;
    thread.start();
#endif
    err = app.exec();
    thread.quit();
    return err;
}
