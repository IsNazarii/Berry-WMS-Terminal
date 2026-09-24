#include "wms/payment.h"
#include "wms/receipt.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace wms;

namespace {

ReceiptInput validInput() {
    return {
        .supplierId = 42,
        .cropCode = "raspberry",
        .gross = grams(25'000),
        .boxCount = 10,
        .tarePerBox = grams(350),
        .pricePerKg = hryvnias(85),
        .priceListVersion = 1,
    };
}

} // namespace

TEST(Receipt, ComputesTareNetAndAmount) {
    const Receipt r = Receipt::create(validInput());
    EXPECT_EQ(r.tare(), grams(3'500));
    EXPECT_EQ(r.net(), grams(21'500));
    EXPECT_EQ(r.amount(), hryvnias(1'827) + kopecks(50)); // 21.5 kg * 85 UAH
}

TEST(Receipt, NewReceiptIsPendingSync) {
    Receipt r = Receipt::create(validInput());
    EXPECT_EQ(r.syncStatus(), SyncStatus::Pending);
    EXPECT_EQ(r.id().version(), 7);
    r.markSynced();
    EXPECT_EQ(r.syncStatus(), SyncStatus::Synced);
}

TEST(Receipt, ValidInputHasNoErrors) { EXPECT_TRUE(Receipt::create(validInput()).isValid()); }

TEST(Receipt, RejectsTareHeavierThanGross) {
    auto input = validInput();
    input.gross = grams(3'000); // less than 10 boxes * 350 g
    EXPECT_FALSE(Receipt::create(input).isValid());
}

TEST(Receipt, ReportsEveryProblem) {
    const Receipt r = Receipt::create({});
    EXPECT_GE(r.validate().size(), 5U);
}

TEST(Receipt, DescribeMarksManualWeight) {
    auto input = validInput();
    input.weightSource = WeightSource::Manual;
    EXPECT_NE(Receipt::create(input).describe().find("ручне"), std::string::npos);
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
    journal.push_back(std::make_unique<Receipt>(Receipt::create(validInput())));
    journal.push_back(std::make_unique<Payment>(Payment::create(42, hryvnias(1'000), PaymentMethod::Cash)));

    Money balance;
    for (const auto& t : journal) balance += t->balanceEffect();

    EXPECT_EQ(balance, hryvnias(827) + kopecks(50));
    EXPECT_EQ(journal[0]->kind(), TransactionKind::Receipt);
    EXPECT_EQ(journal[1]->kind(), TransactionKind::Payment);
}
