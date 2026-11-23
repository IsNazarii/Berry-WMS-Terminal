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

// The README shots: a working day in season, so every screen has something to show.
// Calls go through the same Terminal methods the screens use, and pallets are weighed on
// the scale simulator (stable readings, not typed weights, which the receipt marks).
class ReadmeDay {
public:
    explicit ReadmeDay(QObject* terminal) : terminal_{terminal} {}

    bool ok() const noexcept { return ok_; }

    void play() {
        call("openSession");
        for (auto [crop, price] : {std::pair{"raspberry", "85"}, {"strawberry", "62"}, {"blueberry", "140"}})
            check("setPrice", text("setPrice", crop, price));
        check("addTruck", text4("addTruck", "BI1111AA", "DAF XF", "Іван · Варшава", "1200"));
        check("addTruck", text4("addTruck", "AA7070KT", "MAN TGX", "Петро · Краків", "900"));
        const QString warsaw = destination("BI1111AA");
        const QString krakow = destination("AA7070KT");

        target(warsaw);
        receipt(43, {{"raspberry", 60}, {"raspberry", 56}, {"raspberry", 58}});
        pay(false);
        receipt(44, {{"strawberry", 48}, {"strawberry", 50}});
        pay(true);
        target("warehouse");
        receipt(45, {{"strawberry", 52}, {"strawberry", 46}});
        pay(false);
        target(krakow);
        receipt(77, {{"blueberry", 64}, {"blueberry", 60}, {"raspberry", 58}});
        call("handOverReceipt");  // wholesale: weight only, priced elsewhere
        check("shipFromStock", text3("shipFromStock", "strawberry", "30", krakow));

        // Two receipts still on the scale: one goes to Warsaw, one waits for a place.
        target(warsaw);
        receipt(45, {{"raspberry", 54}});
        target("");
        const QString waiting = receipt(42, {{"raspberry", 60}, {"strawberry", 44}, {"raspberry", 58}});
        target(warsaw);

        // A debt paid in part at the cash desk.
        call("selectCashSupplier", Q_ARG(qint64, 42));
        check("pay", text2("pay", "1500", false));
        QMetaObject::invokeMethod(terminal_, "selectAllocationReceipt", Q_ARG(QString, waiting));
        QMetaObject::invokeMethod(terminal_, "selectHistoryPlace", Q_ARG(QString, "all"));

        // The next pallet already on the scale, settled.
        call("simulatePallet");
        waitStable();
    }

private:
    QString receipt(qint64 supplier, std::initializer_list<std::pair<const char*, int>> pallets) {
        QString error;
        QMetaObject::invokeMethod(terminal_, "startReceipt", Q_RETURN_ARG(QString, error), Q_ARG(qint64, supplier));
        check("startReceipt", error);
        for (auto [crop, boxes] : pallets) {
            call("simulatePallet");
            waitStable();
            QMetaObject::invokeMethod(terminal_, "recordWeighing", Q_RETURN_ARG(QString, error), Q_ARG(QString, crop),
                                      Q_ARG(QString, QString::number(boxes)), Q_ARG(QString, "0,6"),
                                      Q_ARG(QString, "18"), Q_ARG(QString, ""), Q_ARG(bool, false));
            check("recordWeighing", error);
            call("simulateEmptyScale");
            QTest::qWait(400);
        }
        return terminal_->property("activeReceiptId").toString();
    }

    void pay(bool bankTransfer) {
        QVariantList rows;
        QMetaObject::invokeMethod(terminal_, "payRows", Q_RETURN_ARG(QVariantList, rows));
        QVariantMap prices;
        for (const QVariant& row : rows) prices[row.toMap()["cropCode"].toString()] = row.toMap()["price"];
        QString error;
        QMetaObject::invokeMethod(terminal_, "payReceipt", Q_RETURN_ARG(QString, error), Q_ARG(QVariantMap, prices),
                                  Q_ARG(bool, bankTransfer), Q_ARG(bool, false));
        check("payReceipt", error);
    }

    void target(const QString& destinationId) {
        QMetaObject::invokeMethod(terminal_, "setReceivingTarget", Q_ARG(QString, destinationId));
    }

    QString destination(const QString& plate) {
        for (const QVariant& d : terminal_->property("destinations").toList())
            if (d.toMap()["title"].toString().startsWith(plate)) return d.toMap()["destinationId"].toString();
        check("destination", "no truck " + plate);
        return {};
    }

    void waitStable() {
        auto* scales = terminal_->property("scales").value<QObject*>();
        if (!QTest::qWaitFor([&] { return scales->property("stable").toBool() && !scales->property("zero").toBool(); },
                             5000))
            check("scale", "the simulator did not settle");
    }

    template <class... A> void call(const char* method, A... args) {
        QMetaObject::invokeMethod(terminal_, method, args...);
    }
    QString text(const char* m, const QString& a, const QString& b) {
        QString r;
        QMetaObject::invokeMethod(terminal_, m, Q_RETURN_ARG(QString, r), Q_ARG(QString, a), Q_ARG(QString, b));
        return r;
    }
    QString text2(const char* m, const QString& a, bool b) {
        QString r;
        QMetaObject::invokeMethod(terminal_, m, Q_RETURN_ARG(QString, r), Q_ARG(QString, a), Q_ARG(bool, b));
        return r;
    }
    QString text3(const char* m, const QString& a, const QString& b, const QString& c) {
        QString r;
        QMetaObject::invokeMethod(terminal_, m, Q_RETURN_ARG(QString, r), Q_ARG(QString, a), Q_ARG(QString, b),
                                  Q_ARG(QString, c));
        return r;
    }
    QString text4(const char* m, const QString& a, const QString& b, const QString& c, const QString& d) {
        QString r;
        QMetaObject::invokeMethod(terminal_, m, Q_RETURN_ARG(QString, r), Q_ARG(QString, a), Q_ARG(QString, b),
                                  Q_ARG(QString, c), Q_ARG(QString, d));
        return r;
    }
    void check(const char* step, const QString& error) {
        if (error.isEmpty()) return;
        qWarning("wms_shots: %s: %s", step, qPrintable(error));
        ok_ = false;
    }

    QObject* terminal_;
    bool ok_ = true;
};

// The README set: login and the six screens of a populated day, light and dark, one size.
bool takeReadmeShots(QQmlApplicationEngine& engine, const QString& dir) {
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
    auto* terminal = engine.singletonInstance<QObject*>("BerryWms.Ui", "Terminal");
    auto* theme = engine.singletonInstance<QObject*>("BerryWms.Style", "Theme");
    QDir{}.mkpath(dir);
    window->setVisibility(QWindow::Windowed);
    window->resize(1280, 800);
    window->show();
    window->requestActivate();
    auto grab = [&](const QString& name) {
        QTest::qWait(300);
        window->grabWindow().save(dir + "/" + name + ".png");
    };
    auto bothThemes = [&](const QString& name) {
        for (bool dark : {false, true}) {
            theme->setProperty("dark", dark);
            grab(name + (dark ? "-dark" : "-light"));
        }
        theme->setProperty("dark", false);
    };

    bothThemes("login");
    QMetaObject::invokeMethod(terminal, "login", Q_ARG(qint64, 1), Q_ARG(QString, "1111"));
    ReadmeDay day{terminal};
    day.play();
    for (int i = 0; i < 6; ++i) {
        QTest::keyClick(window, Qt::Key(Qt::Key_F5 + i));
        bothThemes(kTabs[i]);
    }
    return day.ok();
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("BerryWms");
    QCoreApplication::setApplicationName("Berry WMS Terminal");
    QGuiApplication app{argc, argv};
    const bool readme = argc == 3 && QByteArray{argv[1]} == "--readme";
    if (argc != 2 && !readme) {
        qWarning("usage: wms_shots [--readme] <dir>");
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

    const QString dir = QString::fromLocal8Bit(argv[argc - 1]);
    QTimer::singleShot(800, &app, [&] {
        QCoreApplication::exit((readme ? takeReadmeShots(engine, dir) : takeShots(engine, dir)) ? 0 : 1);
    });
    return QGuiApplication::exec();
}
