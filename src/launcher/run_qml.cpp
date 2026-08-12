// ============================================================
// run_qml.cpp —— QML 界面运行逻辑（GPU 可选）
//
// 被统一入口 main 调用（--ui=qml）。仅当编译期启用 Qt Quick
// （HAVE_QML）时编译。app 需为 QGuiApplication。
// 复用同一套 core/service 与 SerialWorker。
// ============================================================
#include "launcher/UiRunners.hpp"

#ifdef HAVE_QML

#include "ui/SerialWorker.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int runQml(QGuiApplication& app) {
    // 背靠 SerialWorker（与桌面/TUI/Web 同一套核心）。
    SerialWorker worker;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("serialWorker", &worker);

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

#endif // HAVE_QML