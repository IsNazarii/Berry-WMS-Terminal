#include "wms/payment.h"
#include "wms/receipt.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace wms;

namespace {

// A full pallet of raspberries: 80 boxes on the scale together with the pallet.
Weighing raspberryPallet() {
    return {
        .cropCode = "raspberry",
        .gross = grams(225'000),
        .boxCount = 80,
        .tarePerBox = grams(350),
        .palletTare = grams(25'000),
        .pricePerKg = hryvnias(85),
        .priceListVersion = 1,
    };
}

Weighing blueberryBoxes() {
    return {
        .cropCode = "blueberry",
        .gross = kilograms(20) + grams(33),
        .boxCount = 10,
        .tarePerBox = grams(300),
        .pricePerKg = hryvnias(120) + kopecks(50),
        .priceListVersion = 1,
    };
}

} // namespace

TEST(Weighing, SubtractsBoxAndPalletTare) {
    const Weighing w = raspberryPallet();
    EXPECT_EQ(w.tare(), grams(28'000 + 25'000)); // 80 * 350 g + pallet
    EXPECT_EQ(w.net(), grams(172'000));
    EXPECT_EQ(w.amount(), hryvnias(14'620)); // 172 kg * 85 UAH
    EXPECT_TRUE(w.validate().empty());
}

TEST(Weighing, AmountIsRoundedToKopecks) {
    // 17.033 kg * 120.50 UAH = 2052.4765 UAH
    EXPECT_EQ(blueberryBoxes().amount(), hryvnias(2'052) + kopecks(48));
}

TEST(Weighing, RejectsTareHeavierThanGross) {
    auto w = raspberryPallet();
    w.gross = kilograms(50); // less than boxes + pallet
    EXPECT_FALSE(w.validate().empty());
}

TEST(Weighing, RejectsNegativePalletTare) {
    auto w = raspberryPallet();
    w.palletTare = -grams(1);
    EXPECT_FALSE(w.validate().empty());
}

TEST(Weighing, ReportsEveryProblem) { EXPECT_GE(Weighing{}.validate().size(), 5U); }

TEST(Receipt, SumsSeveralPalletsAndCrops) {
    // Two full pallets and a partial one of raspberries, plus loose blueberries.
    auto partial = raspberryPallet();
    partial.boxCount = 33;
    partial.gross = grams(90'000);
    const Receipt r = Receipt::create(42, {raspberryPallet(), raspberryPallet(), partial, blueberryBoxes()});

    EXPECT_EQ(r.supplierId(), 42);
    EXPECT_EQ(r.weighings().size(), 4U);
    EXPECT_EQ(r.total().boxes, 80 + 80 + 33 + 10);
    EXPECT_EQ(r.net(), grams(172'000 * 2 + 53'450 + 17'033));
    // Each line is rounded on its own, so the total matches the printed lines.
    Money expected;
    for (const auto& w : r.weighings()) expected += w.amount();
    EXPECT_EQ(r.amount(), expected);
    EXPECT_EQ(r.balanceEffect(), r.amount());
}

TEST(Receipt, CargoOfOneCrop) {
    const Receipt r = Receipt::create(42, {raspberryPallet(), blueberryBoxes(), raspberryPallet()});
    EXPECT_EQ(r.cargoOf("raspberry"), (Cargo{.boxes = 160, .weight = kilograms(344)}));
    EXPECT_EQ(r.cargoOf("strawberry"), Cargo{});
}

TEST(Receipt, NewReceiptIsPendingSync) {
    Receipt r = Receipt::create(42, {raspberryPallet()});
    EXPECT_EQ(r.syncStatus(), SyncStatus::Pending);
    EXPECT_EQ(r.id().version(), 7);
    r.markSynced();
    EXPECT_EQ(r.syncStatus(), SyncStatus::Synced);
}

TEST(Receipt, ValidReceiptHasNoErrors) { EXPECT_TRUE(Receipt::create(42, {raspberryPallet()}).isValid()); }

TEST(Receipt, RequiresSupplierAndWeighings) {
    const auto errors = Receipt::create(0, {}).validate();
    EXPECT_EQ(errors.size(), 2U);
}

TEST(Receipt, ErrorsNameTheWeighing) {
    auto bad = raspberryPallet();
    bad.pricePerKg = Money{};
    const auto errors = Receipt::create(42, {raspberryPallet(), bad}).validate();
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("Зважування 2"), std::string::npos);
}

TEST(Receipt, DescribeMarksManualWeight) {
    auto w = raspberryPallet();
    w.weightSource = WeightSource::Manual;
    EXPECT_NE(Receipt::create(42, {w}).describe().find("ручне"), std::string::npos);
}

TEST(Transaction, RejectsNilId) {
    EXPECT_THROW((Payment{Uuid{}, Clock::now(), 1, hryvnias(1), PaymentMethod::Cash}), std::invalid_argument);
}

TEST(Payment, ReducesBalance) {
    const Payment p = Payment::create(42, hryvnias(500), PaymentMethod::Cash);
    EXPECT_EQ(p.balanceEffect(), -hryvnias(500));
    EXPECT_TRUE(p.isValid());
    EXPECT_FALSE(Payment::create(42, Money{}, PaymentMethod::Cash).isValid());
}

TEST(Transaction, BalanceIsSumOfPolymorphicEffects) {
    std::vector<std::unique_ptr<Transaction>> journal;
    journal.push_back(std::make_unique<Receipt>(Receipt::create(42, {raspberryPallet()})));
    journal.push_back(std::make_unique<Payment>(Payment::create(42, hryvnias(1'000), PaymentMethod::Cash)));

    Money balance;
    for (const auto& t : journal) balance += t->balanceEffect();

    EXPECT_EQ(balance, hryvnias(13'620));
    EXPECT_EQ(journal[0]->kind(), TransactionKind::Receipt);
    EXPECT_EQ(journal[1]->kind(), TransactionKind::Payment);
}
