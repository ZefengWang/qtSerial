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
#include <QtQuickControls2/QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int runQml(QGuiApplication& app) {
    // 显式指定 Fusion 原生 style，避免加载 QtQuick.Controls 2.15 的
    // 旧版内部样式实现（在 Qt 6 下会产生大量 "Unknown property
    // transition/content" 的无害噪音警告）。
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    // 背靠 SerialWorker（与桌面/TUI/Web 同一套核心）。
    SerialWorker worker;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("serialWorker", &worker);

    // Main.qml 在 qrc:/qml/，四个页面文件在 qrc:/qml/pages/ 子目录。
    // QML 以裸类型名引用子目录 QML（如 VizPage）时，必须把该目录加入
    // import path，否则在部分构建/部署环境下报 "VizPage is not a type"。
    engine.addImportPath(QStringLiteral("qrc:/qml/pages"));

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