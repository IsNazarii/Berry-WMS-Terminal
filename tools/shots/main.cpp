// Screenshots of the terminal without a window, to compare a UI change before and after:
//   wms_shots <dir>   (run it through tools/shots/shots.sh, which sets up a clean environment)
// Logs in, opens a session and a receipt with one pallet, then for light and dark themes at
// widths 1024 and 1536 visits every tab by its F-key and opens the dialogs of the ⋯ menu.
// Starts the same QML as wms_terminal, with the same style and translations.
#include <QDir>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>
#include <QTranslator>
#include <QtQml/qqmlextensionplugin.h>

#include <utility>

Q_IMPORT_QML_PLUGIN(BerryWms_UiPlugin)
Q_IMPORT_QML_PLUGIN(BerryWms_StylePlugin)

namespace {

constexpr const char* kTabs[] = {"session", "receiving", "allocation", "warehouse", "cash", "history"};

// Returns false when something did not go as the shots assume (a key not reaching its tab,
// a dialog not found), so a script can tell a broken run from a changed picture.
bool takeShots(QQmlApplicationEngine& engine, const QString& dir) {
    bool ok = true;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
    auto* terminal = engine.singletonInstance<QObject*>("BerryWms.Ui", "Terminal");
    auto* theme = engine.singletonInstance<QObject*>("BerryWms.Style", "Theme");
    QDir{}.mkpath(dir);
    window->setVisibility(QWindow::Windowed);
    window->show();
    window->requestActivate();

    auto grab = [&](const QString& name) {
        QTest::qWait(250);  // let bindings, layouts and the scale simulator settle
        window->grabWindow().save(dir + "/" + name + ".png");
    };
    auto dialog = [&](const QString& title) -> QObject* {
        for (QObject* o : window->findChildren<QObject*>())
            if (o->inherits("QQuickDialog") && o->property("title").toString() == title) return o;
        qWarning("wms_shots: no dialog \"%s\"", qPrintable(title));
        ok = false;
        return nullptr;
    };

    window->resize(1024, 700);
    grab("login-1024");

    // The demo operator and supplier from a new database; a typed weight needs no stable scale.
    QString error;
    QMetaObject::invokeMethod(terminal, "login", Q_ARG(qint64, 1), Q_ARG(QString, "1111"));
    QMetaObject::invokeMethod(terminal, "openSession");
    QMetaObject::invokeMethod(terminal, "startReceipt", Q_RETURN_ARG(QString, error), Q_ARG(qint64, 42));
    QMetaObject::invokeMethod(terminal, "recordWeighing", Q_RETURN_ARG(QString, error), Q_ARG(QString, "raspberry"),
                              Q_ARG(QString, "10"), Q_ARG(QString, "1,2"), Q_ARG(QString, "0"),
                              Q_ARG(QString, "40"), Q_ARG(bool, false));
    if (!error.isEmpty()) {
        qWarning("wms_shots: weighing refused: %s", qPrintable(error));
        ok = false;
    }

    auto* bar = window->property("header").value<QQuickItem*>();
    for (bool dark : {false, true}) {
        theme->setProperty("dark", dark);
        for (int width : {1024, 1536}) {
            window->resize(width, width == 1024 ? 700 : 900);
            const QString suffix = QString("-%1-%2").arg(dark ? "dark" : "light").arg(width);
            for (int i = 0; i < 6; ++i) {
                QTest::keyClick(window, Qt::Key(Qt::Key_F5 + i));
                QTest::qWait(50);
                if (bar->property("currentIndex").toInt() != i) {
                    qWarning("wms_shots: F%d did not open tab %d", 5 + i, i);
                    ok = false;
                }
                grab(kTabs[i] + suffix);
            }
            for (auto [title, name] : {std::pair{"Тара ящика за замовчуванням", "dlg-tare"},
                                       std::pair{"Ваги", "dlg-scale"}}) {
                if (QObject* d = dialog(title)) {
                    QMetaObject::invokeMethod(d, "open");
                    grab(name + suffix);
                    QMetaObject::invokeMethod(d, "close");
                }
            }
            QMetaObject::invokeMethod(terminal, "printProblem", Q_ARG(QString, "Принтер не відповідає."));
            grab("dlg-print" + suffix);
            if (QObject* d = dialog("Чек не надруковано")) QMetaObject::invokeMethod(d, "close");
            QTest::qWait(300);  // the closing transition
        }
    }
    return ok;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("BerryWms");
    QCoreApplication::setApplicationName("Berry WMS Terminal");
    QGuiApplication app{argc, argv};
    if (argc != 2) {
        qWarning("usage: wms_shots <dir>");
        return 2;
    }

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale{QLocale::Ukrainian}, "qt", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&qtTranslator);
    QQuickStyle::setStyle("BerryWms.Style");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{"startScale", 1.0}});
    engine.loadFromModule("BerryWms.Ui", "Main");

    const QString dir = QString::fromLocal8Bit(argv[1]);
    QTimer::singleShot(800, &app, [&] { QCoreApplication::exit(takeShots(engine, dir) ? 0 : 1); });
    return QGuiApplication::exec();
}
