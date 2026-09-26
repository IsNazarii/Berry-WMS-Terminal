// Console demo of the domain core. Will be replaced by the Qt/QML terminal UI.
#include "wms/journal.h"
#include "wms/scale.h"
#include "wms/supplier.h"
#include "wms/truck.h"

#include <iostream>
#include <string>
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

    const Receipt receipt = Receipt::create(supplier.id(), Settlement::OnSite, {
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

    Journal journal;

    // describe() is virtual: one printer serves every record type.
    auto print = [](const Transaction& t) { std::cout << t.describe() << "  [" << t.id().toString() << "]\n"; };
    // Generic lambda: journal.add(t) picks the overload for the concrete type at compile time.
    auto record = [&journal, &print](const auto& t) {
        if (const auto errors = t.validate(); !errors.empty()) {
            for (const auto& error : errors) std::cerr << "Помилка: " << error << '\n';
            return false;
        }
        journal.add(t);
        print(t);
        return true;
    };
    // Allocations need the rest of the journal to be checked.
    auto allocate = [&journal, &print](const CrossDockAllocation& a) {
        if (const auto errors = journal.validateAllocation(a); !errors.empty()) {
            for (const auto& error : errors) std::cout << "  відхилено: " << error << '\n';
            return;
        }
        journal.add(a);
        print(a);
    };

    if (!record(receipt)) return 1;
    allocate(CrossDockAllocation::wholeWeighing(receipt, 0, truck.id()));
    allocate(journal.boxesOf(receipt.id(), "blueberry", 3, truck.id()));
    allocate(journal.boxesOf(receipt.id(), "blueberry", 8, truck.id()));    // only 7 left
    allocate(CrossDockAllocation::wholeWeighing(receipt, 0, truck.id()));  // the same pallet again
    allocate(journal.boxesOf(receipt.id(), "blueberry", 7, truck.id()));    // last boxes: exactly the rest

    // A wholesale supplier: the terminal only weighs, a responsible person settles later.
    const bool recorded = record(Receipt::create(77, Settlement::HandedOver, {
        {
            .cropCode = "raspberry",
            .gross = kilograms(210),
            .boxCount = 80,
            .tarePerBox = grams(350),
            .palletTare = kilograms(25),
            .pricePerKg = {}, // agreed later, outside the terminal
            .priceListVersion = 0,
            .weightSource = WeightSource::Manual,
        },
    }));
    if (!recorded || !record(Payment::create(supplier.id(), hryvnias(10'000), PaymentMethod::Cash))) return 1;

    const Cargo load = journal.loadOf(truck.id());
    const Cargo blueberryLeft = journal.unallocatedOf(receipt.id(), "blueberry");
    std::cout << "\nЗаписів у журналі: " << journal.size() << '\n'
              << "Борг перед постачальником: " << formatMoney(journal.balanceOf(supplier.id())) << '\n'
              << "Фура " << truck.plate() << ": " << load.boxes << " ящ., " << formatWeight(load.weight) << " ("
              << truck.fillPercent(load) << "%)\n"
              << "Лохина не розподілена: " << blueberryLeft.boxes << " ящ., " << formatWeight(blueberryLeft.weight)
              << '\n';
    return 0;
}
