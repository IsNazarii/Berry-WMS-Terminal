#include "wms/open_receipt.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

using namespace wms;

namespace {

Weighing pallet(std::string crop, int boxes, Weight net) {
    return {
        .cropCode = std::move(crop),
        .gross = net + grams(300) * boxes + kilograms(25),
        .boxCount = boxes,
        .tarePerBox = grams(300),
        .palletTare = kilograms(25),
        .pricePerKg = hryvnias(85),
    };
}

bool mentions(const std::vector<std::string>& errors, std::string_view text) {
    return std::ranges::any_of(errors, [&](const std::string& e) { return e.find(text) != std::string::npos; });
}

} // namespace

TEST(OpenReceipt, OpensEmptyWithItsFinalId) {
    const OpenReceipt r = OpenReceipt::open(42);
    EXPECT_EQ(r.supplierId(), 42);
    EXPECT_FALSE(r.id().isNil());
    EXPECT_TRUE(r.weighings().empty());
}

TEST(OpenReceipt, RequiresSupplier) { EXPECT_THROW((void)OpenReceipt::open(0), std::invalid_argument); }

TEST(OpenReceipt, RestoredFromStorageAsItWas) {
    const Uuid id = Uuid::generateV7();
    const TimePoint openedAt = Clock::now();
    const OpenReceipt r{id, openedAt, 42, {pallet("raspberry", 80, kilograms(160))}};
    EXPECT_EQ(r.id(), id);
    EXPECT_EQ(r.openedAt(), openedAt);
    EXPECT_EQ(r.weighings().size(), 1U);
}

TEST(OpenReceipt, RestoreRejectsNilId) {
    EXPECT_THROW((OpenReceipt{Uuid{}, Clock::now(), 42, {}}), std::invalid_argument);
}

TEST(OpenReceipt, AddAppendsAndReturnsTheIndex) {
    OpenReceipt r = OpenReceipt::open(42);
    EXPECT_EQ(r.add(pallet("raspberry", 80, kilograms(160))), 0U);
    EXPECT_EQ(r.add(pallet("blueberry", 40, kilograms(70))), 1U);
    EXPECT_EQ(r.weighings()[1].cropCode, "blueberry");
}

TEST(OpenReceipt, AddRejectsInvalidWeighing) {
    // The UI shows Weighing::validate() to the operator first; getting here is a bug.
    OpenReceipt r = OpenReceipt::open(42);
    EXPECT_THROW(r.add(pallet("raspberry", 0, kilograms(160))), std::invalid_argument);
    EXPECT_TRUE(r.weighings().empty());
}

TEST(OpenReceipt, CancelKeepsTheLineButNotItsCargo) {
    OpenReceipt r = OpenReceipt::open(42);
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.add(pallet("raspberry", 33, kilograms(66)));
    r.cancel(0);

    ASSERT_EQ(r.weighings().size(), 2U);
    EXPECT_TRUE(r.weighings()[0].cancelled);
    EXPECT_EQ(r.total(), (Cargo{.boxes = 33, .weight = kilograms(66)}));
}

TEST(OpenReceipt, CancelOutsideTheReceiptThrows) {
    OpenReceipt r = OpenReceipt::open(42);
    EXPECT_THROW(r.cancel(0), std::out_of_range);
}

TEST(OpenReceipt, ClosedReceiptKeepsIdOpeningTimeAndLines) {
    // Allocations made while it was open already point at this id and these indices.
    OpenReceipt r = OpenReceipt::open(42);
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.add(pallet("blueberry", 40, kilograms(70)));
    r.cancel(0);

    const Receipt closed = r.close(Settlement::OnSite);
    EXPECT_EQ(closed.id(), r.id());
    EXPECT_EQ(closed.createdAt(), r.openedAt());
    EXPECT_EQ(closed.supplierId(), 42);
    EXPECT_EQ(closed.settlement(), Settlement::OnSite);
    ASSERT_EQ(closed.weighings().size(), 2U);
    EXPECT_TRUE(closed.weighings()[0].cancelled);
    EXPECT_EQ(closed.amount(), hryvnias(85 * 70));
}

TEST(OpenReceipt, HandedOverDropsDayPrices) {
    // Weighings took the day price, but a wholesale receipt is priced outside the terminal.
    OpenReceipt r = OpenReceipt::open(42);
    r.add(pallet("raspberry", 80, kilograms(160)));

    EXPECT_TRUE(r.validateClose(Settlement::HandedOver).empty());
    const Receipt closed = r.close(Settlement::HandedOver);
    EXPECT_TRUE(closed.isValid());
    EXPECT_TRUE(closed.weighings()[0].pricePerKg.isZero());
    EXPECT_TRUE(closed.amount().isZero());
}

TEST(OpenReceipt, OnSiteNeedsPrices) {
    OpenReceipt r = OpenReceipt::open(42);
    auto noPrice = pallet("raspberry", 80, kilograms(160));
    noPrice.pricePerKg = Money{};
    r.add(noPrice);
    EXPECT_TRUE(mentions(r.validateClose(Settlement::OnSite), "Ціна"));
}

TEST(OpenReceipt, CannotCloseWithoutWeighings) {
    OpenReceipt r = OpenReceipt::open(42);
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.cancel(0);

    EXPECT_TRUE(mentions(r.validateClose(Settlement::OnSite), "жодного зважування"));
    EXPECT_THROW((void)r.close(Settlement::OnSite), std::logic_error);
}

TEST(OpenReceipt, CropsAreListedOnceInWeighingOrder) {
    OpenReceipt r = OpenReceipt::open(42);
    r.add(pallet("strawberry", 40, kilograms(80)));
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.add(pallet("strawberry", 40, kilograms(80)));
    r.add(pallet("blueberry", 10, kilograms(20)));
    r.cancel(3);

    EXPECT_EQ(r.crops(), (std::vector<std::string>{"strawberry", "raspberry"}));
}

TEST(OpenReceipt, SetPriceRepricesEveryCountedLineOfTheCrop) {
    OpenReceipt r = OpenReceipt::open(42);
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.add(pallet("strawberry", 40, kilograms(80)));
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.cancel(3);

    r.setPrice("raspberry", hryvnias(90), PriceSource::Manual);

    const auto& lines = r.weighings();
    EXPECT_EQ(lines[0].pricePerKg, hryvnias(90));
    EXPECT_EQ(lines[2].pricePerKg, hryvnias(90));
    EXPECT_EQ(lines[2].priceSource, PriceSource::Manual);
    EXPECT_EQ(lines[1].pricePerKg, hryvnias(85));
    // The cancelled line keeps what it was weighed with: it is a trace of the mistake.
    EXPECT_EQ(lines[3].pricePerKg, hryvnias(85));
    EXPECT_EQ(lines[3].priceSource, PriceSource::DayPrice);
}

TEST(OpenReceipt, ClosedReceiptKeepsItsSession) {
    const Uuid session = Uuid::generateV7();
    OpenReceipt r = OpenReceipt::open(42, session);
    EXPECT_EQ(r.sessionId(), session);
    r.add(pallet("raspberry", 80, kilograms(150)));
    EXPECT_EQ(r.close(Settlement::OnSite).sessionId(), session);
    EXPECT_FALSE(OpenReceipt::open(42).sessionId().has_value());
}
