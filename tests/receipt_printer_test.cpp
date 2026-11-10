#include "wms/qr_code.h"
#include "wms/receipt_printer.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace wms;

namespace {

const TimePoint kAt = TimePoint{} + std::chrono::hours{500'000};
constexpr double kDpi = 100;
// A4 at kDpi with a margin the drawing must stay out of.
constexpr int kPageWidth = 827;
constexpr int kPageHeight = 1169;
constexpr int kMargin = 47;

ReceiptDocument documentWith(int pallets, Settlement settlement = Settlement::OnSite, std::string supplier = "Шевченко Іван") {
    std::vector<Weighing> weighings;
    for (int i = 0; i < pallets; ++i) {
        weighings.push_back({
            .cropCode = i % 2 == 0 ? "raspberry" : "strawberry",
            .gross = kilograms(130),
            .boxCount = 10,
            .tarePerBox = kilograms(1),
            .palletTare = kilograms(20),
            .pricePerKg = settlement == Settlement::OnSite ? hryvnias(90) : Money{},
        });
    }
    const Receipt receipt{Uuid::parse("019a3b5c-7d1e-7f00-8a2b-1234567890ab").value(), kAt, 42, settlement,
                          std::move(weighings)};
    const std::optional<ReceiptPaymentInfo> payment =
        settlement == Settlement::OnSite
            ? std::optional{ReceiptPaymentInfo{.method = PaymentMethod::Cash, .amount = receipt.amount(), .at = kAt, .by = "Оксана"}}
            : std::nullopt;
    return makeReceiptDocument(receipt, Supplier{42, std::move(supplier)},
                               {{"raspberry", "Малина"}, {"strawberry", "Полуниця"}}, payment,
                               {.at = kAt, .by = "Оксана", .copy = false});
}

bool hasInk(const QImage& image, const QRect& region) {
    for (int y = region.top(); y <= region.bottom(); ++y) {
        for (int x = region.left(); x <= region.right(); ++x) {
            if (image.pixelColor(x, y) != QColor{Qt::white}) return true;
        }
    }
    return false;
}

// Draws the document sheet by sheet and returns the sheets.
std::vector<QImage> render(const ReceiptDocument& doc, int pageWidth = kPageWidth, int pageHeight = kPageHeight) {
    std::vector<QImage> sheets;
    QImage image{pageWidth, pageHeight, QImage::Format_RGB32};
    image.fill(Qt::white);
    QPainter painter{&image};
    const QRectF area{kMargin, kMargin, double(pageWidth - 2 * kMargin), double(pageHeight - 2 * kMargin)};
    paintReceipt(painter, area, kDpi, doc, [&] {
        sheets.push_back(image.copy());
        image.fill(Qt::white);
    });
    painter.end();
    sheets.push_back(image);
    return sheets;
}

} // namespace

TEST(QrMatrix, AReceiptIdFitsAVersion3Symbol) {
    // 36 characters in byte mode at level M: version 3, 29 modules per side. Pinned, because
    // the printed size of a module, and so whether a scanner reads it, depends on it.
    EXPECT_EQ(QrMatrix{"019a3b5c-7d1e-7f00-8a2b-1234567890ab"}.size(), 29);
}

TEST(QrMatrix, HasFinderPatternsInThreeCornersOnly) {
    const QrMatrix qr{"019a3b5c-7d1e-7f00-8a2b-1234567890ab"};
    const int last = qr.size() - 1;
    // Corner of a finder pattern: dark, then the light ring from the seventh module on.
    EXPECT_TRUE(qr.dark(0, 0));
    EXPECT_FALSE(qr.dark(7, 0));
    EXPECT_TRUE(qr.dark(last, 0));
    EXPECT_FALSE(qr.dark(last - 7, 0));
    EXPECT_TRUE(qr.dark(0, last));
    EXPECT_FALSE(qr.dark(0, last - 7));
    // The middle of a finder pattern is a solid 3×3 square.
    EXPECT_TRUE(qr.dark(3, 3));
}

TEST(QrMatrix, DifferentTextDifferentSymbol) {
    const QrMatrix a{"019a3b5c-7d1e-7f00-8a2b-1234567890ab"};
    const QrMatrix b{"019a3b5c-7d1e-7f00-8a2b-1234567890ac"};
    ASSERT_EQ(a.size(), b.size());
    bool differs = false;
    for (int y = 0; y < a.size(); ++y) {
        for (int x = 0; x < a.size(); ++x) differs = differs || a.dark(x, y) != b.dark(x, y);
    }
    EXPECT_TRUE(differs);
}

TEST(QrMatrix, OutsideTheSymbolThrows) {
    const QrMatrix qr{"x"};
    EXPECT_THROW((void)qr.dark(qr.size(), qr.size()), std::out_of_range);
}

TEST(PaintReceipt, AnOrdinaryReceiptIsOneSheet) {
    EXPECT_EQ(render(documentWith(10)).size(), 1U);
}

TEST(PaintReceipt, ALongReceiptGoesOnToMoreSheets) {
    EXPECT_GE(render(documentWith(100)).size(), 3U);
}

TEST(PaintReceipt, NothingIsDrawnOutsideTheAreaAtAnyLength) {
    // Every length, so that the seam where the totals just fit or just move on is covered.
    for (int pallets = 1; pallets <= 90; ++pallets) {
        for (const QImage& sheet : render(documentWith(pallets))) {
            ASSERT_FALSE(hasInk(sheet, QRect{0, kPageHeight - kMargin + 1, kPageWidth, kMargin - 1}))
                << pallets << " pallets ran over the bottom";
            ASSERT_FALSE(hasInk(sheet, QRect{0, 0, kMargin - 1, kPageHeight}))
                << pallets << " pallets ran over the left edge";
            ASSERT_FALSE(hasInk(sheet, QRect{kPageWidth - kMargin + 1, 0, kMargin - 1, kPageHeight}))
                << pallets << " pallets ran over the right edge";
        }
    }
}

TEST(PaintReceipt, EverySheetHasSomethingOnIt) {
    for (int pallets = 1; pallets <= 90; ++pallets) {
        for (const QImage& sheet : render(documentWith(pallets))) {
            ASSERT_TRUE(hasInk(sheet, sheet.rect())) << pallets << " pallets left an empty sheet";
        }
    }
}

TEST(PaintReceipt, QrCodeIsInTheTopRightCorner) {
    const QImage sheet = render(documentWith(1)).front();
    // With a quiet zone of 4 modules and 29 modules across 32 mm.
    EXPECT_TRUE(hasInk(sheet, QRect{kPageWidth - kMargin - 100, kMargin, 100, 100}));
}

TEST(PaintReceipt, ALongNameStaysOutOfTheQrCode) {
    const QImage plain = render(documentWith(1)).front();
    const QImage named = render(documentWith(1, Settlement::OnSite, std::string(400, 'W'))).front();
    // The QR column, 32 mm wide, is the same with any name.
    const QRect qrColumn{kPageWidth - kMargin - 126, kMargin, 126, 126};
    EXPECT_EQ(plain.copy(qrColumn), named.copy(qrColumn));
}

TEST(PaintReceipt, FitsA5AtAnyLength) {
    constexpr int kA5Width = 583;
    constexpr int kA5Height = 827;
    for (int pallets = 1; pallets <= 60; ++pallets) {
        for (const QImage& sheet : render(documentWith(pallets), kA5Width, kA5Height)) {
            ASSERT_FALSE(hasInk(sheet, QRect{0, kA5Height - kMargin + 1, kA5Width, kMargin - 1}))
                << pallets << " pallets ran over the bottom";
            ASSERT_FALSE(hasInk(sheet, QRect{0, 0, kMargin - 1, kA5Height}))
                << pallets << " pallets ran over the left edge";
            ASSERT_FALSE(hasInk(sheet, QRect{kA5Width - kMargin + 1, 0, kMargin - 1, kA5Height}))
                << pallets << " pallets ran over the right edge";
            ASSERT_TRUE(hasInk(sheet, sheet.rect())) << pallets << " pallets left an empty sheet";
        }
    }
}

TEST(PaintReceipt, AnOrdinaryReceiptFitsOneA5Sheet) {
    EXPECT_EQ(render(documentWith(10), 583, 827).size(), 1U);
}

TEST(PaintReceipt, WholesaleIsDrawnToo) {
    EXPECT_EQ(render(documentWith(10, Settlement::HandedOver)).size(), 1U);
}

TEST(ReceiptPrinter, WritesAPdfIntoTheFolder) {
    // WMS_RECEIPT_SAMPLES keeps the files, to look at the real thing.
    const QString samples = qEnvironmentVariable("WMS_RECEIPT_SAMPLES");
    QTemporaryDir temporary;
    const QString folder = samples.isEmpty() ? temporary.path() : samples;

    const ReceiptPrinter printer{folder};
    const ReceiptDocument paid = documentWith(6);
    EXPECT_EQ(printer.print(paid), QString{});
    const QString file = QDir{folder}.filePath("receipt-019a3b5c-7d1e-7f00-8a2b-1234567890ab.pdf");
    EXPECT_GT(QFile{file}.size(), 1000);

    if (!samples.isEmpty()) {
        ReceiptDocument wholesale = documentWith(6, Settlement::HandedOver);
        wholesale.printed.copy = true;
        EXPECT_EQ(printer.print(wholesale), QString{});
        ReceiptDocument longOne = documentWith(60);
        longOne.printed.copy = false;
        const ReceiptPrinter longPrinter{QDir{samples}.filePath("long")};
        EXPECT_EQ(longPrinter.print(longOne), QString{});
        const ReceiptPrinter a5Printer{QDir{samples}.filePath("a5")};
        EXPECT_EQ(a5Printer.print(paid, PaperSize::A5), QString{});
        const ReceiptPrinter a5LongPrinter{QDir{samples}.filePath("a5-long")};
        EXPECT_EQ(a5LongPrinter.print(longOne, PaperSize::A5), QString{});
    }
}

int main(int argc, char** argv) {
    // Fonts need a GUI application, but no screen.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    // The font of the real receipt, so the widths in the tests are the real ones.
    for (const char* weight : {"Regular", "Medium", "SemiBold", "Bold"}) {
        QFontDatabase::addApplicationFont(QString{WMS_FONT_DIR "/Onest-%1.ttf"}.arg(weight));
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
