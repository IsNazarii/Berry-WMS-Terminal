// Terminal entry point: starts the QML UI. The console demo of the core that
// used to live here is in git history (commit 0ab1548).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlextensionplugin.h>

// The UI module is a static library, so its QML plugin must be imported explicitly.
Q_IMPORT_QML_PLUGIN(BerryWms_UiPlugin)

int main(int argc, char* argv[]) {
    QGuiApplication app{argc, argv};
    QGuiApplication::setApplicationName("Berry WMS Terminal");

    QQmlApplicationEngine engine;
    // A broken QML file must not leave an invisible running process.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("BerryWms.Ui", "Main");

    return QGuiApplication::exec();
}
