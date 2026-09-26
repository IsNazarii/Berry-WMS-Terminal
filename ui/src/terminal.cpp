#include "terminal.h"

#include "wms/payment.h"
#include "wms/receipt.h"

#include <algorithm>

namespace wms::ui {
namespace {

// Reference data the server will send later; hard-coded for the alpha.
std::vector<Supplier> demoSuppliers() {
    return {
        {42, "Шевченко Тарас", "A", "+380671112233"},
        {43, "Коваленко Олена", "A", "+380502223344"},
        {44, "Бондар Петро", "B"},
        {45, "Мельник Ірина", "B", "+380933334455"},
        {77, "ФГ «Лосятинські ягоди»", "опт"},
    };
}

std::vector<Truck> demoTrucks() {
    return {
        {7, "BI1234AA", {.boxes = 1'200, .weight = kilograms(2'000)}},
        {8, "BI5678BB", {.boxes = 0, .weight = kilograms(5'000)}},
    };
}

// History from an earlier day, so the cash screen has a debt to show.
void addDemoHistory(Journal& journal) {
    journal.add(Receipt::create(42, Settlement::OnSite, {{
        .cropCode = "raspberry",
        .gross = kilograms(120),
        .boxCount = 40,
        .tarePerBox = grams(350),
        .palletTare = kilograms(25),
        .pricePerKg = hryvnias(85),
        .priceListVersion = 1,
    }}));
    journal.add(Payment::create(42, hryvnias(3'000), PaymentMethod::Cash));
}

} // namespace

Terminal::Terminal(QObject* parent)
    : QObject{parent},
      suppliers_{demoSuppliers()},
      trucks_{demoTrucks()},
      supplierModel_{suppliers_, journal_} {
    addDemoHistory(journal_);
    connect(this, &Terminal::journalChanged, &supplierModel_, &SupplierListModel::refreshBalances);
}

int Terminal::journalSize() const noexcept { return static_cast<int>(journal_.size()); }

QString Terminal::supplierName() const {
    const Supplier* supplier = currentSupplier();
    return supplier ? QString::fromStdString(supplier->fullName()) : QString{};
}

QString Terminal::supplierBalance() const {
    if (!currentSupplierId_) return {};
    return QString::fromStdString(formatMoney(journal_.balanceOf(*currentSupplierId_)));
}

bool Terminal::selectSupplier(qint64 id) {
    const auto found = std::ranges::find(suppliers_, id, &Supplier::id);
    if (found == suppliers_.end()) return false;
    if (currentSupplierId_ != id) {
        currentSupplierId_ = id;
        emit currentSupplierChanged();
        emit balanceChanged();
    }
    return true;
}

void Terminal::clearSupplier() {
    if (!currentSupplierId_) return;
    currentSupplierId_.reset();
    emit currentSupplierChanged();
    emit balanceChanged();
}

const Supplier* Terminal::currentSupplier() const noexcept {
    if (!currentSupplierId_) return nullptr;
    const auto found = std::ranges::find(suppliers_, *currentSupplierId_, &Supplier::id);
    return found != suppliers_.end() ? &*found : nullptr;
}

} // namespace wms::ui
