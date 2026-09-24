// Console demo of the domain core. Will be replaced by the Qt/QML terminal UI.
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/scale.h"
#include "wms/supplier.h"

#include <iostream>
#include <memory>
#include <vector>

int main() {
    using namespace wms;

    const Supplier supplier{42, "Шевченко Тарас", "A"};
    std::cout << "Постачальник: " << supplier.fullName() << " (категорія " << supplier.priceCategory() << ")\n";

    // The scale swings while boxes are being placed, then settles.
    SimulatedScaleDriver scale{{grams(3'000), grams(21'400), grams(24'950), grams(25'010), grams(25'000),
                                grams(24'990)}};
    StabilityDetector detector{3, grams(20)};

    scale.open();
    std::optional<Weight> gross;
    while (!gross) {
        const auto reading = scale.read();
        if (!reading) break;
        detector.push(*reading);
        std::cout << "  ваги: " << formatWeight(*reading) << (detector.isStable() ? "  [стабільно]" : "") << '\n';
        gross = detector.stableWeight();
    }
    scale.close();

    if (!gross) {
        std::cerr << "Вага не стабілізувалась\n";
        return 1;
    }

    std::vector<std::unique_ptr<Transaction>> journal;
    journal.push_back(std::make_unique<Receipt>(Receipt::create({
        .supplierId = supplier.id(),
        .cropCode = "raspberry",
        .gross = *gross,
        .boxCount = 10,
        .tarePerBox = grams(350),
        .pricePerKg = hryvnias(85),
        .priceListVersion = 1,
    })));
    journal.push_back(std::make_unique<Payment>(Payment::create(supplier.id(), hryvnias(1'500), PaymentMethod::Cash)));

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
