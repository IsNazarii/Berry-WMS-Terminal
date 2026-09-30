// Terminal entry point: starts the QML UI. The console demo of the core that
// used to live here is in git history (commit 0ab1548).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

// The UI modules are static libraries, so their QML plugins must be imported explicitly.
Q_IMPORT_QML_PLUGIN(BerryWms_UiPlugin)
Q_IMPORT_QML_PLUGIN(BerryWms_StylePlugin)

int main(int argc, char* argv[]) {
    QGuiApplication app{argc, argv};
    QGuiApplication::setApplicationName("Berry WMS Terminal");

    // Our own controls style (ui/style); it must be chosen before the engine loads any QML.
    QQuickStyle::setStyle("BerryWms.Style");

    QQmlApplicationEngine engine;
    // A broken QML file must not leave an invisible running process.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("BerryWms.Ui", "Main");

    return QGuiApplication::exec();
}
