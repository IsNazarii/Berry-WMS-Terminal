#include "wms/receipt_document.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace wms;

namespace {

const TimePoint kAt = TimePoint{} + std::chrono::hours{500'000};
const Supplier kSupplier{42, "Шевченко Іван"};
const std::vector<Crop> kCrops{{"raspberry", "Малина"}, {"strawberry", "Полуниця"}};

Weighing pallet(std::string crop, int boxes, Weight gross, Money price, bool cancelled = false) {
    return {
        .cropCode = std::move(crop),
        .gross = gross,
        .boxCount = boxes,
        .tarePerBox = kilograms(1),
        .palletTare = kilograms(20),
        .pricePerKg = price,
        .cancelled = cancelled,
    };
}

Receipt receiptOf(Settlement settlement, std::vector<Weighing> weighings) {
    return {Uuid::generateV7(), kAt, kSupplier.id(), settlement, std::move(weighings)};
}

ReceiptDocument documentOf(const Receipt& receipt, const std::optional<ReceiptPaymentInfo>& payment = std::nullopt) {
    return makeReceiptDocument(receipt, kSupplier, kCrops, payment, {.at = kAt, .by = "Оксана", .copy = false});
}

} // namespace

TEST(ReceiptDocument, CarriesWhoWhenAndTheId) {
    const Receipt receipt = receiptOf(Settlement::OnSite, {pallet("raspberry", 10, kilograms(130), hryvnias(90))});
    const ReceiptDocument doc = documentOf(receipt);
    EXPECT_EQ(doc.receiptId, receipt.id());
    EXPECT_EQ(doc.receivedAt, kAt);
    EXPECT_EQ(doc.supplierId, 42);
    EXPECT_EQ(doc.supplierName, "Шевченко Іван");
    EXPECT_EQ(doc.printed.by, "Оксана");
}

TEST(ReceiptDocument, LineShowsTheWeighingInFull) {
    const ReceiptDocument doc =
        documentOf(receiptOf(Settlement::OnSite, {pallet("raspberry", 10, kilograms(130), hryvnias(90))}));
    ASSERT_EQ(doc.lines.size(), 1U);
    const ReceiptLine& line = doc.lines[0];
    EXPECT_EQ(line.number, 1);
    EXPECT_EQ(line.crop, "Малина");
    EXPECT_EQ(line.boxes, 10);
    EXPECT_EQ(line.gross, kilograms(130));
    EXPECT_EQ(line.tare, kilograms(30)); // 10 boxes of 1 kg + a 20 kg pallet
    EXPECT_EQ(line.net, kilograms(100));
    EXPECT_EQ(line.pricePerKg, hryvnias(90));
    EXPECT_EQ(line.amount, hryvnias(9000));
}

TEST(ReceiptDocument, CancelledLinesAreNotPrintedAndTheNumbersHaveNoGap) {
    const ReceiptDocument doc = documentOf(receiptOf(
        Settlement::OnSite, {pallet("raspberry", 10, kilograms(130), hryvnias(90)),
                             pallet("raspberry", 99, kilograms(999), hryvnias(90), /*cancelled=*/true),
                             pallet("strawberry", 5, kilograms(75), hryvnias(60))}));
    ASSERT_EQ(doc.lines.size(), 2U);
    EXPECT_EQ(doc.lines[1].number, 2);
    EXPECT_EQ(doc.lines[1].crop, "Полуниця");
    EXPECT_EQ(doc.total.boxes, 15);
    EXPECT_EQ(doc.total.weight, kilograms(150));
    EXPECT_EQ(doc.amount, hryvnias(12000));
}

TEST(ReceiptDocument, CropTotalsFollowTheOrderOfWeighing) {
    const ReceiptDocument doc = documentOf(receiptOf(
        Settlement::OnSite, {pallet("strawberry", 5, kilograms(75), hryvnias(60)),
                             pallet("raspberry", 10, kilograms(130), hryvnias(90)),
                             pallet("strawberry", 5, kilograms(75), hryvnias(60))}));
    ASSERT_EQ(doc.cropTotals.size(), 2U);
    EXPECT_EQ(doc.cropTotals[0].crop, "Полуниця");
    EXPECT_EQ(doc.cropTotals[0].cargo.boxes, 10);
    EXPECT_EQ(doc.cropTotals[0].cargo.weight, kilograms(100));
    EXPECT_EQ(doc.cropTotals[0].amount, hryvnias(6000));
    EXPECT_EQ(doc.cropTotals[1].crop, "Малина");
}

TEST(ReceiptDocument, ACropOnlyInCancelledLinesHasNoTotal) {
    const ReceiptDocument doc = documentOf(
        receiptOf(Settlement::OnSite, {pallet("strawberry", 5, kilograms(75), hryvnias(60), /*cancelled=*/true),
                                       pallet("raspberry", 10, kilograms(130), hryvnias(90))}));
    ASSERT_EQ(doc.cropTotals.size(), 1U);
    EXPECT_EQ(doc.cropTotals[0].crop, "Малина");
}

TEST(ReceiptDocument, WholesaleHasNoPrices) {
    const ReceiptDocument doc =
        documentOf(receiptOf(Settlement::HandedOver, {pallet("raspberry", 10, kilograms(130), Money{})}));
    EXPECT_FALSE(doc.priced());
    EXPECT_EQ(doc.amount, Money{});
    EXPECT_EQ(doc.total.weight, kilograms(100));
}

TEST(ReceiptDocument, ACropMissingFromTheCatalogueShowsItsCode) {
    // E.g. a record synced in before its crop.
    const ReceiptDocument doc =
        documentOf(receiptOf(Settlement::OnSite, {pallet("blueberry", 10, kilograms(130), hryvnias(90))}));
    EXPECT_EQ(doc.lines[0].crop, "blueberry");
}

TEST(ReceiptDocument, KeepsThePaymentAndTheCopyMark) {
    const Receipt receipt = receiptOf(Settlement::OnSite, {pallet("raspberry", 10, kilograms(130), hryvnias(90))});
    const ReceiptDocument doc = makeReceiptDocument(
        receipt, kSupplier, kCrops,
        ReceiptPaymentInfo{.method = PaymentMethod::BankTransfer, .amount = hryvnias(9000), .at = kAt, .by = "Петро"},
        {.at = kAt, .by = "Оксана", .copy = true});
    ASSERT_TRUE(doc.payment.has_value());
    EXPECT_EQ(doc.payment->method, PaymentMethod::BankTransfer);
    EXPECT_EQ(doc.payment->by, "Петро");
    EXPECT_TRUE(doc.printed.copy);
}
