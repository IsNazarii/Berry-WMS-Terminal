// Console demo of the domain core. Will be replaced by the Qt/QML terminal UI.
#include "wms/allocation.h"
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/scale.h"
#include "wms/supplier.h"
#include "wms/truck.h"

#include <iostream>
#include <memory>
#include <vector>

int main() {
    using namespace wms;

    const Supplier supplier{42, "Шевченко Тарас", "A"};
    std::cout << "Постачальник: " << supplier.fullName() << " (категорія " << supplier.priceCategory() << ")\n";

    StabilityDetector detector{3, grams(20)};

    // The scale swings while a pallet is being placed, then settles.
    auto weighStable = [&detector](std::vector<Weight> script) -> std::optional<Weight> {
        SimulatedScaleDriver scale{std::move(script)};
        detector.reset();
        scale.open();
        std::optional<Weight> stable;
        for (int i = 0; i < 20 && !stable; ++i) {
            const auto reading = scale.read();
            if (!reading) break;
            detector.push(*reading);
            std::cout << "  ваги: " << formatWeight(*reading) << (detector.isStable() ? "  [стабільно]" : "") << '\n';
            stable = detector.stableWeight();
        }
        return stable;
    };

    const auto palletGross = weighStable({kilograms(40), kilograms(198), grams(224'990), grams(225'010), kilograms(225)});
    const auto blueberryGross = weighStable({kilograms(5), grams(20'020), grams(20'040), grams(20'030)});
    if (!palletGross || !blueberryGross) {
        std::cerr << "Вага не стабілізувалась\n";
        return 1;
    }

    const Truck truck{7, "BI1234AA", {.boxes = 1'200, .weight = kilograms(2'000)}};

    const Receipt receipt = Receipt::create(supplier.id(), {
        {
            .cropCode = "raspberry",
            .gross = *palletGross,
            .boxCount = 80,
            .tarePerBox = grams(350),
            .palletTare = kilograms(25),
            .pricePerKg = hryvnias(85),
            .priceListVersion = 1,
        },
        {
            .cropCode = "blueberry",
            .gross = *blueberryGross,
            .boxCount = 10,
            .tarePerBox = grams(300),
            .palletTare = {}, // weighed without a pallet
            .pricePerKg = hryvnias(120) + kopecks(50),
            .priceListVersion = 1,
        },
    });

    std::vector<std::unique_ptr<Transaction>> journal;
    journal.push_back(std::make_unique<Receipt>(receipt));
    journal.push_back(std::make_unique<CrossDockAllocation>(CrossDockAllocation::wholeWeighing(receipt, 0, truck.id())));
    journal.push_back(std::make_unique<CrossDockAllocation>(CrossDockAllocation::boxesOf(receipt, "blueberry", 4, truck.id())));
    journal.push_back(std::make_unique<Payment>(Payment::create(supplier.id(), hryvnias(10'000), PaymentMethod::Cash)));

    Money balance;
    for (const auto& transaction : journal) {
        if (const auto errors = transaction->validate(); !errors.empty()) {
            for (const auto& error : errors) std::cerr << "Помилка: " << error << '\n';
            return 1;
        }
        std::cout << transaction->describe() << "  [" << transaction->id().toString() << "]\n";
        balance += transaction->balanceEffect();
    }
    std::cout << "Борг перед постачальником: " << formatMoney(balance) << '\n';
    return 0;
}
