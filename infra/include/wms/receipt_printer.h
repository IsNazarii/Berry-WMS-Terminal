#pragma once

#include "wms/receipt_document.h"

#include <QRectF>
#include <QString>

#include <functional>

class QPainter;

namespace wms {

enum class PaperSize { A4, A5 };

// Draws the receipt on any QPainter: one sheet, more when the lines do not fit. An area
// narrower than 150 mm (A5) gets everything smaller, in one proportion.
// area is in the painter's coordinates, dotsPerInch turns millimetres and points into
// them. newPage is called before each further sheet; afterwards drawing starts over at
// the top of area. Apart from the printer, so tests can look at the result.
void paintReceipt(QPainter& painter, const QRectF& area, double dotsPerInch, const ReceiptDocument& doc,
                  const std::function<void()>& newPage);

// Sends receipts to the system's default printer through Qt (CUPS on Linux).
class ReceiptPrinter {
public:
    // A folder makes PDF files in it instead of printing: for a terminal without a
    // printer, and for trying things out without wasting paper.
    explicit ReceiptPrinter(QString pdfFolder = {});

    // What the operator should know, empty when printing went well. The job is only
    // handed to the print queue here: paper can still run out later.
    [[nodiscard]] QString print(const ReceiptDocument& doc, PaperSize paper = PaperSize::A4) const;

private:
    QString pdfFolder_;
};

} // namespace wms
