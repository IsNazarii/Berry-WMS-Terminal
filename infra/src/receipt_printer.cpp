#include "wms/receipt_printer.h"

#include "wms/display_format.h"
#include "wms/qr_code.h"

#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QFontMetricsF>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QPrinter>
#include <QPrinterInfo>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace wms {
namespace {

QString text(const std::string& utf8) { return QString::fromStdString(utf8); }

QString kilogramsText(Weight weight) { return text(displayKilograms(weight)); }
QString hryvniasText(Money money) { return text(displayHryvnias(money)); }

QString dateTimeText(TimePoint at) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(at.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("dd.MM.yyyy HH:mm"));
}

struct Column {
    QString title;
    double share; // of the table's width
    Qt::Alignment align;
};

const std::array kPricedColumns{
    Column{QStringLiteral("№"), 0.06, Qt::AlignLeft},
    Column{QStringLiteral("Культура"), 0.18, Qt::AlignLeft},
    Column{QStringLiteral("Ящ."), 0.07, Qt::AlignRight},
    Column{QStringLiteral("Брутто, кг"), 0.13, Qt::AlignRight},
    Column{QStringLiteral("Тара, кг"), 0.11, Qt::AlignRight},
    Column{QStringLiteral("Нетто, кг"), 0.13, Qt::AlignRight},
    Column{QStringLiteral("Ціна, грн/кг"), 0.17, Qt::AlignRight},
    Column{QStringLiteral("Сума, грн"), 0.15, Qt::AlignRight},
};
// Wholesale: the same columns without money, so the paper never hints at a price.
const std::array kWeightColumns{
    Column{QStringLiteral("№"), 0.08, Qt::AlignLeft},
    Column{QStringLiteral("Культура"), 0.36, Qt::AlignLeft},
    Column{QStringLiteral("Ящ."), 0.10, Qt::AlignRight},
    Column{QStringLiteral("Брутто, кг"), 0.16, Qt::AlignRight},
    Column{QStringLiteral("Тара, кг"), 0.14, Qt::AlignRight},
    Column{QStringLiteral("Нетто, кг"), 0.16, Qt::AlignRight},
};

// All sizes in one place, in the units of paper; the sheet is drawn top to bottom,
// y_ being where the next thing goes.
class Sheet {
public:
    Sheet(QPainter& painter, const QRectF& area, double dotsPerInch, const ReceiptDocument& doc,
          const std::function<void()>& newPage)
        : painter_{painter},
          area_{area},
          dotsPerInch_{dotsPerInch},
          doc_{doc},
          newPage_{newPage},
          // Everything but the width of the table shrinks together, so the proportions of the sheet hold.
          scale_{area.width() * 25.4 / dotsPerInch < 150 ? 0.85 : 1.0},
          columns_{doc.priced() ? std::span<const Column>{kPricedColumns} : std::span<const Column>{kWeightColumns}},
          y_{area.top()} {
        painter_.setPen(Qt::black);
    }

    void draw() {
        drawHeading();
        drawTableHeader();
        for (const ReceiptLine& line : doc_.lines) {
            if (y_ + mm(kRowMm) > area_.bottom()) continueOnNextSheet(true);
            drawLine(line);
        }
        // The totals and signatures stay together: a signature alone on a sheet proves nothing.
        if (y_ + tailHeight() > area_.bottom()) continueOnNextSheet(false);
        drawTotals();
        drawPayment();
        drawSignatures();
        drawPrintedNote();
    }

private:
    static constexpr double kRowMm = 6.5;
    static constexpr double kBodyPt = 10;

    [[nodiscard]] double mm(double millimetres) const { return millimetres * scale_ * dotsPerInch_ / 25.4; }

    // In pixels of the device rather than points: the same size on the printer and on a test image.
    [[nodiscard]] QFont font(double points, QFont::Weight weight = QFont::Normal) const {
        QFont f{QStringLiteral("Onest")};
        f.setPixelSize(std::max(1, static_cast<int>(std::lround(points * scale_ * dotsPerInch_ / 72))));
        f.setWeight(weight);
        return f;
    }

    // One line of text in the given box; what does not fit ends with "…" rather than running over the next cell.
    // For words only: a shortened number on a money document would be worse than an untidy one, see drawRow().
    void cell(const QRectF& box, Qt::Alignment align, const QString& value) {
        const QString fitted = QFontMetricsF{painter_.font(), painter_.device()}.elidedText(
            value, Qt::ElideRight, box.width());
        painter_.drawText(box, align | Qt::AlignVCenter | Qt::TextSingleLine, fitted);
    }

    void rule(double width) {
        painter_.setPen(QPen{Qt::black, width});
        painter_.drawLine(QPointF{area_.left(), y_}, QPointF{area_.right(), y_});
        painter_.setPen(Qt::black);
    }

    void drawHeading() {
        const double qrSide = mm(32);
        drawQr(QRectF{area_.right() - qrSide, area_.top(), qrSide, qrSide});
        const double width = area_.width() - qrSide - mm(4);

        painter_.setFont(font(18, QFont::Bold));
        cell(QRectF{area_.left(), y_, width, mm(10)}, Qt::AlignLeft, QStringLiteral("Чек прийому ягід"));
        y_ += mm(12);
        if (doc_.printed.copy) {
            painter_.setFont(font(12, QFont::Bold));
            cell(QRectF{area_.left(), y_, width, mm(6)}, Qt::AlignLeft, QStringLiteral("КОПІЯ"));
            y_ += mm(6);
        }
        const double label = mm(32);
        const auto info = [&](const QString& name, const QString& value) {
            painter_.setFont(font(kBodyPt));
            cell(QRectF{area_.left(), y_, label, mm(6)}, Qt::AlignLeft, name);
            painter_.setFont(font(kBodyPt, QFont::DemiBold));
            cell(QRectF{area_.left() + label, y_, width - label, mm(6)}, Qt::AlignLeft, value);
            y_ += mm(6);
        };
        info(QStringLiteral("Постачальник"),
             QStringLiteral("%1 (№ %2)").arg(text(doc_.supplierName)).arg(doc_.supplierId));
        info(QStringLiteral("Прийнято"), dateTimeText(doc_.receivedAt));
        info(QStringLiteral("Розрахунок"), doc_.priced() ? QStringLiteral("роздріб") : QStringLiteral("опт"));
        if (!doc_.priced()) {
            painter_.setFont(font(kBodyPt - 1));
            cell(QRectF{area_.left(), y_, width, mm(6)}, Qt::AlignLeft,
                 QStringLiteral("Лише вага: ціну й розрахунок визначає відповідальна особа."));
            y_ += mm(6);
        }
        y_ = std::max(y_, area_.top() + qrSide) + mm(1);
        painter_.setFont(font(kBodyPt - 2));
        cell(QRectF{area_.left(), y_, area_.width(), mm(5)}, Qt::AlignLeft,
             QStringLiteral("Номер чека: %1").arg(text(doc_.receiptId.toString())));
        y_ += mm(8);
    }

    void drawQr(const QRectF& box) {
        const QrMatrix qr{doc_.receiptId.toString()};
        // Four light modules around the symbol are part of the QR standard, scanners need them.
        constexpr int kQuietZone = 4;
        // Whole device pixels per module: at a fractional size the modules get uneven, which scanners dislike.
        const int module = std::max(1, static_cast<int>(box.width() / (qr.size() + 2 * kQuietZone)));
        const double side = module * qr.size();
        const double left = std::round(box.right() - side - kQuietZone * module);
        const double top = std::round(box.top() + kQuietZone * module);
        painter_.save();
        painter_.setRenderHint(QPainter::Antialiasing, false);
        for (int y = 0; y < qr.size(); ++y) {
            for (int x = 0; x < qr.size(); ++x) {
                if (qr.dark(x, y)) painter_.fillRect(QRectF{left + x * module, top + y * module,
                                                            double(module), double(module)}, Qt::black);
            }
        }
        painter_.restore();
    }

    void drawRow(const std::span<const QString> values) {
        double x = area_.left();
        const double padding = mm(1);
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            const double width = columns_[i].share * area_.width();
            const QRectF box{x + padding, y_, width - 2 * padding, mm(kRowMm)};
            if (columns_[i].align & Qt::AlignRight) {
                // Numbers are never shortened: a too wide one grows to the left, into the empty side of its neighbour.
                painter_.drawText(box, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip, values[i]);
            } else {
                cell(box, columns_[i].align, values[i]);
            }
            x += width;
        }
        y_ += mm(kRowMm);
    }

    void drawTableHeader() {
        rule(mm(0.4));
        painter_.setFont(font(kBodyPt - 1, QFont::DemiBold));
        std::vector<QString> titles;
        for (const Column& column : columns_) titles.push_back(column.title);
        drawRow(titles);
        rule(mm(0.4));
    }

    void drawLine(const ReceiptLine& line) {
        painter_.setFont(font(kBodyPt));
        std::vector<QString> values{QString::number(line.number), text(line.crop), QString::number(line.boxes),
                                    kilogramsText(line.gross), kilogramsText(line.tare), kilogramsText(line.net)};
        if (doc_.priced()) {
            values.push_back(hryvniasText(line.pricePerKg));
            values.push_back(hryvniasText(line.amount));
        }
        drawRow(values);
        rule(mm(0.1));
    }

    void drawTotalRow(const QString& name, const Cargo& cargo, Money amount) {
        std::vector<QString> values{QString{}, name, QString::number(cargo.boxes), QString{}, QString{},
                                    kilogramsText(cargo.weight)};
        if (doc_.priced()) {
            values.emplace_back();
            values.push_back(hryvniasText(amount));
        }
        drawRow(values);
    }

    // With one crop its total would only repeat "Всього".
    [[nodiscard]] bool showsCropTotals() const noexcept { return doc_.cropTotals.size() > 1; }

    void drawTotals() {
        rule(mm(0.4));
        if (showsCropTotals()) {
            painter_.setFont(font(kBodyPt));
            for (const ReceiptCropTotal& crop : doc_.cropTotals) drawTotalRow(text(crop.crop), crop.cargo, crop.amount);
            rule(mm(0.1));
        }
        painter_.setFont(font(kBodyPt + 1, QFont::Bold));
        drawTotalRow(QStringLiteral("Всього"), doc_.total, doc_.amount);
        rule(mm(0.4));
    }

    void drawPayment() {
        if (!doc_.payment) return;
        y_ += mm(3);
        const ReceiptPaymentInfo& paid = *doc_.payment;
        const QString how = paid.method == PaymentMethod::Cash ? QStringLiteral("готівкою") : QStringLiteral("переказом");
        painter_.setFont(font(kBodyPt + 1, QFont::Bold));
        cell(QRectF{area_.left(), y_, area_.width(), mm(7)}, Qt::AlignLeft,
             QStringLiteral("Виплачено %1: %2 грн").arg(how, hryvniasText(paid.amount)));
        y_ += mm(7);
        painter_.setFont(font(kBodyPt - 1));
        cell(QRectF{area_.left(), y_, area_.width(), mm(5)}, Qt::AlignLeft,
             QStringLiteral("%1, видав(ла): %2").arg(dateTimeText(paid.at), text(paid.by)));
        y_ += mm(5);
    }

    void drawSignatures() {
        y_ += mm(16);
        const double width = (area_.width() - mm(10)) / 2;
        const auto signature = [&](double x, const QString& who) {
            painter_.setPen(QPen{Qt::black, mm(0.2)});
            painter_.drawLine(QPointF{x, y_}, QPointF{x + width, y_});
            painter_.setPen(Qt::black);
            painter_.setFont(font(kBodyPt - 2));
            cell(QRectF{x, y_ + mm(0.5), width, mm(4.5)}, Qt::AlignLeft, who);
        };
        signature(area_.left(), QStringLiteral("Постачальник (підпис)"));
        signature(area_.left() + width + mm(10), QStringLiteral("Оператор (підпис)"));
        y_ += mm(5);
    }

    void drawPrintedNote() {
        y_ += mm(4);
        painter_.setFont(font(kBodyPt - 2));
        QString note = QStringLiteral("Надруковано %1, оператор: %2")
                           .arg(dateTimeText(doc_.printed.at), text(doc_.printed.by));
        if (doc_.printed.copy) note += QStringLiteral(" · копія");
        cell(QRectF{area_.left(), y_, area_.width(), mm(5)}, Qt::AlignLeft, note);
        y_ += mm(5);
    }

    // Everything after the lines, in the order of draw(); it must fit one sheet whole.
    [[nodiscard]] double tailHeight() const {
        const auto cropRows = showsCropTotals() ? static_cast<double>(doc_.cropTotals.size()) : 0.0;
        const double totals = (cropRows + 1) * kRowMm;
        const double payment = doc_.payment ? 3 + 7 + 5 : 0;
        const double signatures = 16 + 5;
        const double printedNote = 4 + 5;
        return mm(totals + payment + signatures + printedNote);
    }

    void continueOnNextSheet(bool moreLines) {
        newPage_();
        ++sheet_;
        y_ = area_.top();
        painter_.setFont(font(kBodyPt - 1));
        QString title = QStringLiteral("Чек %1 · %2 · аркуш %3")
                            .arg(text(doc_.receiptId.toString()), text(doc_.supplierName)).arg(sheet_);
        if (doc_.printed.copy) title += QStringLiteral(" · КОПІЯ");
        cell(QRectF{area_.left(), y_, area_.width(), mm(6)}, Qt::AlignLeft, title);
        y_ += mm(8);
        if (moreLines) drawTableHeader();
    }

    QPainter& painter_;
    const QRectF area_;
    const double dotsPerInch_;
    const ReceiptDocument& doc_;
    const std::function<void()>& newPage_;
    const double scale_;
    const std::span<const Column> columns_;
    double y_;
    int sheet_ = 1;
};

} // namespace

void paintReceipt(QPainter& painter, const QRectF& area, double dotsPerInch, const ReceiptDocument& doc,
                  const std::function<void()>& newPage) {
    Sheet{painter, area, dotsPerInch, doc, newPage}.draw();
}

ReceiptPrinter::ReceiptPrinter(QString pdfFolder) : pdfFolder_{std::move(pdfFolder)} {}

QString ReceiptPrinter::print(const ReceiptDocument& doc, PaperSize paper) const {
    QPrinter printer{QPrinter::HighResolution};
    const QString id = text(doc.receiptId.toString());
    if (pdfFolder_.isEmpty()) {
        const QPrinterInfo target = QPrinterInfo::defaultPrinter();
        if (target.isNull()) return QStringLiteral("у системі не налаштовано принтера");
        // A stopped queue would still take the job and print it whenever the printer comes back,
        // maybe next week, long after the supplier left. Better to say so now.
        if (target.state() == QPrinter::Error) {
            return QStringLiteral("принтер %1 вимкнений або не підключений").arg(target.printerName());
        }
        printer.setPrinterName(target.printerName());
    } else {
        if (!QDir{}.mkpath(pdfFolder_)) return QStringLiteral("не вдалося створити теку %1").arg(pdfFolder_);
        printer.setOutputFormat(QPrinter::PdfFormat);
        const QString name = doc.printed.copy ? QStringLiteral("receipt-%1-copy.pdf") : QStringLiteral("receipt-%1.pdf");
        printer.setOutputFileName(QDir{pdfFolder_}.filePath(name.arg(id)));
    }
    printer.setDocName(QStringLiteral("Чек %1").arg(id));
    const bool a5 = paper == PaperSize::A5;
    printer.setPageSize(QPageSize{a5 ? QPageSize::A5 : QPageSize::A4});
    const double margin = a5 ? 10 : 12;
    printer.setPageMargins(QMarginsF{margin, margin, margin, margin}, QPageLayout::Millimeter);
    printer.setColorMode(QPrinter::GrayScale);

    QPainter painter;
    if (!painter.begin(&printer)) return QStringLiteral("принтер не прийняв завдання");
    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    paintReceipt(painter, QRectF{0, 0, page.width(), page.height()}, printer.resolution(), doc,
                 [&printer] { printer.newPage(); });
    if (!painter.end() || printer.printerState() == QPrinter::Error) {
        return QStringLiteral("принтер не прийняв завдання");
    }
    return {};
}

} // namespace wms
