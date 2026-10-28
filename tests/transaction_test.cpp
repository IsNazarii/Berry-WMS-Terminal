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
    };
}

Weighing blueberryBoxes() {
    return {
        .cropCode = "blueberry",
        .gross = kilograms(20) + grams(33),
        .boxCount = 10,
        .tarePerBox = grams(300),
        .pricePerKg = hryvnias(120) + kopecks(50),
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

TEST(Weighing, ReportsEveryWeightProblem) {
    // Price is not checked here: whether it is required depends on the receipt's settlement.
    EXPECT_GE(Weighing{}.validate().size(), 4U);
}

TEST(Receipt, SumsSeveralPalletsAndCrops) {
    // Two full pallets and a partial one of raspberries, plus loose blueberries.
    auto partial = raspberryPallet();
    partial.boxCount = 33;
    partial.gross = grams(90'000);
    const Receipt r = Receipt::create(42, Settlement::OnSite, {raspberryPallet(), raspberryPallet(), partial, blueberryBoxes()});

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
    const Receipt r = Receipt::create(42, Settlement::OnSite, {raspberryPallet(), blueberryBoxes(), raspberryPallet()});
    EXPECT_EQ(r.cargoOf("raspberry"), (Cargo{.boxes = 160, .weight = kilograms(344)}));
    EXPECT_EQ(r.cargoOf("strawberry"), Cargo{});
}

TEST(Receipt, NewReceiptIsPendingSync) {
    Receipt r = Receipt::create(42, Settlement::OnSite, {raspberryPallet()});
    EXPECT_EQ(r.syncStatus(), SyncStatus::Pending);
    EXPECT_EQ(r.id().version(), 7);
    r.markSynced();
    EXPECT_EQ(r.syncStatus(), SyncStatus::Synced);
}

TEST(Receipt, ValidReceiptHasNoErrors) { EXPECT_TRUE(Receipt::create(42, Settlement::OnSite, {raspberryPallet()}).isValid()); }

TEST(Receipt, RequiresSupplierAndWeighings) {
    const auto errors = Receipt::create(0, Settlement::OnSite, {}).validate();
    EXPECT_EQ(errors.size(), 2U);
}

TEST(Receipt, ErrorsNameTheWeighing) {
    auto bad = raspberryPallet();
    bad.pricePerKg = Money{};
    const auto errors = Receipt::create(42, Settlement::OnSite, {raspberryPallet(), bad}).validate();
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("Зважування 2"), std::string::npos);
}

TEST(Receipt, OnSiteRequiresPrice) {
    auto w = raspberryPallet();
    w.pricePerKg = Money{};
    EXPECT_FALSE(Receipt::create(42, Settlement::OnSite, {w}).isValid());
}

namespace {

// Wholesale: the price is agreed later by a responsible person, the terminal only weighs.
Weighing wholesalePallet() {
    auto w = raspberryPallet();
    w.pricePerKg = Money{};
    return w;
}

} // namespace

TEST(Receipt, HandedOverRecordsWeightOnly) {
    const Receipt r = Receipt::create(42, Settlement::HandedOver, {wholesalePallet(), wholesalePallet()});
    EXPECT_EQ(r.settlement(), Settlement::HandedOver);
    EXPECT_TRUE(r.isValid());
    EXPECT_EQ(r.total(), (Cargo{.boxes = 160, .weight = kilograms(344)}));
    EXPECT_TRUE(r.amount().isZero());
    // Settled outside the terminal, so the operator's supplier balance is unchanged.
    EXPECT_TRUE(r.balanceEffect().isZero());
}

TEST(Receipt, HandedOverRejectsPrice) {
    // A price on a wholesale receipt would print an amount nobody agreed to.
    const auto errors = Receipt::create(42, Settlement::HandedOver, {wholesalePallet(), raspberryPallet()}).validate();
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("Зважування 2"), std::string::npos);
}

TEST(Receipt, HandedOverStillValidatesWeight) {
    auto w = wholesalePallet();
    w.boxCount = 0;
    EXPECT_FALSE(Receipt::create(42, Settlement::HandedOver, {w}).isValid());
}

TEST(Receipt, DescribeHandedOverShowsNoMoney) {
    const auto text = Receipt::create(42, Settlement::HandedOver, {wholesalePallet()}).describe();
    EXPECT_NE(text.find("до розрахунку"), std::string::npos);
    EXPECT_EQ(text.find("грн"), std::string::npos);
}

TEST(Receipt, RetailDebtStaysUntilPaidInParts) {
    // No cash on the day of reception: the debt waits in the journal and is paid later in parts.
    std::vector<std::unique_ptr<Transaction>> journal;
    journal.push_back(std::make_unique<Receipt>(Receipt::create(42, Settlement::OnSite, {raspberryPallet()})));
    Money balance = journal[0]->balanceEffect();
    EXPECT_EQ(balance, hryvnias(14'620));

    journal.push_back(std::make_unique<Payment>(Payment::create(42, hryvnias(10'000), PaymentMethod::Cash, 1)));
    journal.push_back(std::make_unique<Payment>(Payment::create(42, hryvnias(4'620), PaymentMethod::BankTransfer, 1)));
    balance = Money{};
    for (const auto& t : journal) balance += t->balanceEffect();
    EXPECT_TRUE(balance.isZero());
}

TEST(Receipt, DescribeMarksManualWeight) {
    auto w = raspberryPallet();
    w.weightSource = WeightSource::Manual;
    EXPECT_NE(Receipt::create(42, Settlement::OnSite, {w}).describe().find("ручне"), std::string::npos);
}

namespace {

// A pallet weighed into the wrong receipt by mistake.
Weighing cancelled(Weighing w) {
    w.cancelled = true;
    return w;
}

} // namespace

TEST(Receipt, CancelledWeighingIsNotCounted) {
    const Receipt r = Receipt::create(42, Settlement::OnSite, {raspberryPallet(), cancelled(blueberryBoxes())});
    EXPECT_EQ(r.total(), raspberryPallet().cargo());
    EXPECT_EQ(r.amount(), raspberryPallet().amount());
    EXPECT_EQ(r.cargoOf("blueberry"), Cargo{});
}

TEST(Receipt, CancelledWeighingKeepsItsPlaceAndValues) {
    // Allocations point at weighings by index, so a cancelled line must not disappear.
    const Receipt r = Receipt::create(42, Settlement::OnSite, {cancelled(blueberryBoxes()), raspberryPallet()});
    ASSERT_EQ(r.weighings().size(), 2U);
    EXPECT_TRUE(r.weighings()[0].cancelled);
    EXPECT_EQ(r.weighings()[0].net(), blueberryBoxes().net());
}

TEST(Receipt, CancelledWeighingIsNotValidated) {
    // A mistaken line need not be correct, it only has to stay on record.
    auto wrong = raspberryPallet();
    wrong.boxCount = 0;
    EXPECT_TRUE(Receipt::create(42, Settlement::OnSite, {raspberryPallet(), cancelled(wrong)}).isValid());
}

TEST(Receipt, OnlyCancelledWeighingsAreNoReceipt) {
    const auto errors = Receipt::create(42, Settlement::OnSite, {cancelled(raspberryPallet())}).validate();
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("жодного зважування"), std::string::npos);
}

TEST(Receipt, DescribeMarksCancelledWeighing) {
    const auto text = Receipt::create(42, Settlement::OnSite, {raspberryPallet(), cancelled(blueberryBoxes())}).describe();
    EXPECT_NE(text.find("анульовано"), std::string::npos);
}

TEST(Transaction, RejectsNilId) {
    EXPECT_THROW((Payment{Uuid{}, Clock::now(), 1, hryvnias(1), PaymentMethod::Cash, 1}), std::invalid_argument);
}

TEST(Payment, ReducesBalance) {
    const Payment p = Payment::create(42, hryvnias(500), PaymentMethod::Cash, 1);
    EXPECT_EQ(p.balanceEffect(), -hryvnias(500));
    EXPECT_TRUE(p.isValid());
    EXPECT_FALSE(Payment::create(42, Money{}, PaymentMethod::Cash, 1).isValid());
}

// Cash leaving the till must name who handed it over.
TEST(Payment, RemembersWhoPaid) {
    EXPECT_EQ(Payment::create(42, hryvnias(500), PaymentMethod::Cash, 2).paidBy(), 2);
    EXPECT_THROW((void)Payment::create(42, hryvnias(500), PaymentMethod::Cash, 0), std::invalid_argument);
}

TEST(Transaction, BalanceIsSumOfPolymorphicEffects) {
    std::vector<std::unique_ptr<Transaction>> journal;
    journal.push_back(std::make_unique<Receipt>(Receipt::create(42, Settlement::OnSite, {raspberryPallet()})));
    journal.push_back(std::make_unique<Payment>(Payment::create(42, hryvnias(1'000), PaymentMethod::Cash, 1)));

    Money balance;
    for (const auto& t : journal) balance += t->balanceEffect();

    EXPECT_EQ(balance, hryvnias(13'620));
    EXPECT_EQ(journal[0]->kind(), TransactionKind::Receipt);
    EXPECT_EQ(journal[1]->kind(), TransactionKind::Payment);
}

TEST(Weighing, PriceComesFromDayPricesByDefault) {
    EXPECT_EQ(raspberryPallet().priceSource, PriceSource::DayPrice);
}

TEST(Weighing, ManualPriceCountsLikeDayPrice) {
    // A one-off raise for one supplier: marked internally, priced the usual way.
    auto raised = raspberryPallet();
    raised.pricePerKg = hryvnias(88);
    raised.priceSource = PriceSource::Manual;
    EXPECT_EQ(raised.amount(), costOf(raised.net(), hryvnias(88)));

    auto sameAtDayPrice = raised;
    sameAtDayPrice.priceSource = PriceSource::DayPrice;
    const Uuid id = Uuid::generateV7();
    const TimePoint at = Clock::now();
    const Receipt manual{id, at, 42, Settlement::OnSite, {raised}};
    const Receipt usual{id, at, 42, Settlement::OnSite, {sameAtDayPrice}};
    EXPECT_TRUE(manual.isValid());
    // The supplier must not see the difference.
    EXPECT_EQ(manual.describe(), usual.describe());
    EXPECT_EQ(manual.amount(), usual.amount());
}

TEST(Receipt, BelongsToTheSessionItWasWeighedIn) {
    const Uuid session = Uuid::generateV7();
    const Receipt receipt{Uuid::generateV7(), Clock::now(), 42, Settlement::OnSite, {raspberryPallet()},
                          SyncStatus::Pending, session};
    EXPECT_EQ(receipt.sessionId(), session);
    EXPECT_FALSE(Receipt::create(42, Settlement::OnSite, {raspberryPallet()}).sessionId().has_value());
}

TEST(Payment, KnowsItsSessionOrNone) {
    // The cash desk works without an open session: such a payment belongs to no work day.
    EXPECT_FALSE(Payment::create(42, hryvnias(100), PaymentMethod::Cash, 1).sessionId().has_value());
    const Uuid session = Uuid::generateV7();
    EXPECT_EQ(Payment::create(42, hryvnias(100), PaymentMethod::Cash, 1, session).sessionId(), session);
}
