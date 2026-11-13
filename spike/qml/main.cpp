// Spike: the QML history screen alone in a window, for pictures next to the Widgets one.
#include "../demo.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(BerryWms_UiPlugin)
Q_IMPORT_QML_PLUGIN(BerryWms_StylePlugin)

int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("BerryWms");
    QCoreApplication::setApplicationName("Berry WMS Terminal");
    QGuiApplication app{argc, argv};
    QQuickStyle::setStyle("BerryWms.Style");

    QQmlApplicationEngine engine;
    engine.loadData(R"(
        import QtQuick
        import QtQuick.Controls
        import BerryWms.Ui
        ApplicationWindow {
            visible: true
            color: Theme.bg
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            HistoryScreen { anchors.fill: parent }
        })");
    auto* t = engine.singletonInstance<wms::ui::Terminal*>("BerryWms.Ui", "Terminal");
    if (qEnvironmentVariableIsSet("WMS_DARK")) {
        auto* theme = engine.singletonInstance<QObject*>("BerryWms.Style", "Theme");
        theme->setProperty("dark", qEnvironmentVariable("WMS_DARK") == "1");
    }
    if (qEnvironmentVariableIsSet("WMS_DEMO")) playDemo(*t);
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    window->resize(qEnvironmentVariableIntegerValue("WMS_WIDTH").value_or(1360), 712);

    if (qEnvironmentVariableIsSet("WMS_SHOT")) {
        if (const QString place = qEnvironmentVariable("WMS_PLACE"); !place.isEmpty())
            t->selectHistoryPlace(place == "truck" ? t->historyPlaces()[1].toMap()["placeId"].toString() : place);
        QTimer::singleShot(500, [=] {
            window->grabWindow().save(qEnvironmentVariable("WMS_SHOT"));
            QGuiApplication::quit();
        });
    }
    return QGuiApplication::exec();
}
