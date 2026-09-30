// Terminal entry point: starts the QML UI. The console demo of the core that
// used to live here is in git history (commit 0ab1548).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSettings>
#include <QtQml/qqmlextensionplugin.h>

// The UI modules are static libraries, so their QML plugins must be imported explicitly.
Q_IMPORT_QML_PLUGIN(BerryWms_UiPlugin)
Q_IMPORT_QML_PLUGIN(BerryWms_StylePlugin)

namespace {

// Qt follows the system display scale, but GNOME's "large text" only reaches GTK
// apps, and at 100% a 15" Full HD laptop makes the terminal too small. So the
// terminal keeps its own scale (footer menu, key "ui/scale", same default as
// Main.qml). Qt reads it only at startup, before the application object exists;
// a QT_SCALE_FACTOR set by hand still wins. Returns the scale in effect.
double applyUiScale() {
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) return qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();
    const double scale = QSettings{}.value("ui/scale", 1.25).toDouble();
    if (scale <= 0) return 1;
    qputenv("QT_SCALE_FACTOR", QByteArray::number(scale));
    return scale;
}

} // namespace

int main(int argc, char* argv[]) {
    // Before the application object: QSettings needs the names, the scale must be set first.
    QCoreApplication::setOrganizationName("BerryWms");
    QCoreApplication::setApplicationName("Berry WMS Terminal");
    const double uiScale = applyUiScale();

    QGuiApplication app{argc, argv};

    // Our own controls style (ui/style); it must be chosen before the engine loads any QML.
    QQuickStyle::setStyle("BerryWms.Style");

    QQmlApplicationEngine engine;
    // A broken QML file must not leave an invisible running process.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{"startScale", uiScale}});
    engine.loadFromModule("BerryWms.Ui", "Main");

    return QGuiApplication::exec();
}
