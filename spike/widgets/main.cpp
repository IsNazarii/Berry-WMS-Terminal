// Spike: the history screen on Qt Widgets, fed by the same Terminal as the QML UI.
#include "history_screen.h"
#include "theme.h"

#include "../demo.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("BerryWms");
    QCoreApplication::setApplicationName("Berry WMS Terminal");
    QApplication app{argc, argv};
    spike::loadFonts();
    spike::setTheme(qEnvironmentVariable("WMS_DARK") == "1");

    wms::ui::Terminal terminal;
    if (qEnvironmentVariableIsSet("WMS_DEMO")) playDemo(terminal);

    QWidget window;
    window.setObjectName("window");
    window.setAttribute(Qt::WA_StyledBackground);
    auto* screen = new spike::HistoryScreen{terminal, &window};
    window.resize(qEnvironmentVariableIntegerValue("WMS_WIDTH").value_or(1360), 712);
    screen->resize(window.size());
    window.show();

    // WMS_SHOT=file.png: a picture of the window, then quit.
    if (qEnvironmentVariableIsSet("WMS_SHOT")) {
        if (const QString place = qEnvironmentVariable("WMS_PLACE"); !place.isEmpty())
            terminal.selectHistoryPlace(place == "truck" ? terminal.historyPlaces()[1].toMap()["placeId"].toString()
                                                         : place);
        QTimer::singleShot(300, [&] {
            window.grab().save(qEnvironmentVariable("WMS_SHOT"));
            QApplication::quit();
        });
    }
    return QApplication::exec();
}
