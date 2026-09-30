#include "terminal.h"

#include "display_format.h"

#include "wms/allocation.h"
#include "wms/payment.h"
#include "wms/receipt.h"

#include <QDateTime>
#include <QRandomGenerator>

#include <algorithm>
#include <chrono>
#include <format>

namespace wms::ui {
namespace {

// Reference data the server will send later; hard-coded for the alpha.
std::vector<Operator> demoOperators() {
    return {
        {1, "Назарій", "1111"},
        {2, "Оксана", "2222"},
    };
}

std::vector<Supplier> demoSuppliers() {
    return {
        {42, "Шевченко Тарас", "+380671112233"},
        {43, "Коваленко Олена", "+380502223344"},
        {44, "Бондар Петро"},
        {45, "Мельник Ірина", "+380933334455"},
        {77, "ФГ «Лосятинські ягоди»"},
    };
}

struct Crop {
    const char* code;
    const char* name;
};

// Crops of the Losiatyn point; a crop catalogue comes with the server.
constexpr Crop kCrops[] = {
    {"raspberry", "Малина"},
    {"strawberry", "Полуниця"},
    {"blueberry", "Лохина"},
};

// Trucks of yesterday's session: one left, one was carried over half-loaded.
std::vector<Truck> demoPreviousTrucks() {
    std::vector<Truck> trucks{
        Truck::create("BI1234AA", {.boxes = 1'200, .weight = kilograms(2'000)}),
        Truck::create("BI5678BB", {.boxes = 0, .weight = kilograms(5'000)}),
    };
    trucks[0].depart();
    trucks[1].carryOver();
    return trucks;
}

// History from an earlier day, so the cash screen has a debt to show and the
// carried-over truck already has a pallet in it.
void addDemoHistory(Journal& journal, const TruckId& carriedOverTruck) {
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {{
        .cropCode = "raspberry",
        .gross = kilograms(120),
        .boxCount = 40,
        .tarePerBox = grams(350),
        .palletTare = kilograms(25),
        .pricePerKg = hryvnias(85),
    }});
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, carriedOverTruck));
    journal.add(Payment::create(42, hryvnias(3'000), PaymentMethod::Cash));

    // Strawberries that waited overnight in the warehouse: stock outlives the session.
    const Receipt stored = Receipt::create(45, Settlement::OnSite, {{
        .cropCode = "strawberry",
        .gross = kilograms(90),
        .boxCount = 30,
        .tarePerBox = grams(300),
        .palletTare = kilograms(20),
        .pricePerKg = hryvnias(60),
    }});
    journal.add(stored);
    journal.add(Allocation::wholeWeighing(stored, 0, ToWarehouse{}));
}

QString localTime(TimePoint at) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(at.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms).toString("HH:mm");
}

// "340 ящ. · 120.000 кг"; a zero part is left out, since for a limit it means "no limit".
QString describeCargo(const Cargo& cargo) {
    QStringList parts;
    if (cargo.boxes != 0) parts << QString("%1 ящ.").arg(cargo.boxes);
    if (!cargo.weight.isZero()) parts << QString::fromStdString(formatWeight(cargo.weight));
    return parts.isEmpty() ? QString("0") : parts.join(" · ");
}

QString statusText(TruckStatus status) {
    switch (status) {
    case TruckStatus::Loading: return QObject::tr("завантажується");
    case TruckStatus::Departed: return QObject::tr("відправлено");
    case TruckStatus::CarriedOver: return QObject::tr("перенесено в наступну сесію");
    }
    return {};
}

// A limit field: empty means no limit, otherwise a non-negative whole number.
std::optional<std::int64_t> parseLimit(const QString& text) {
    if (text.trimmed().isEmpty()) return 0;
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    return ok && value >= 0 ? std::optional<std::int64_t>{value} : std::nullopt;
}

QString cropName(std::string_view code) {
    const auto found = std::ranges::find(kCrops, code, &Crop::code);
    return found != std::end(kCrops) ? QString::fromUtf8(found->name) : QString::fromUtf8(code);
}

// Money of the lines that count; the receipt's own amount() exists only once it is closed.
Money amountOf(const OpenReceipt& receipt) {
    Money sum;
    for (const Weighing& w : receipt.weighings()) {
        if (!w.cancelled) sum += w.amount();
    }
    return sum;
}

QString money(Money value) { return QString::fromStdString(formatMoney(value)); }
QString weight(Weight value) { return QString::fromStdString(formatWeight(value)); }

// As the operator types it, so "pay the whole debt" can fill the amount field.
QString moneyInput(Money value) {
    return QString("%1,%2").arg(value.raw() / 100).arg(value.raw() % 100, 2, 10, QChar('0'));
}

QString localDateTime(TimePoint at) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(at.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms).toString("dd.MM HH:mm");
}

QString joined(const std::vector<std::string>& errors) {
    QStringList list;
    for (const std::string& error : errors) list << QString::fromStdString(error);
    return list.join("\n");
}

// The two kinds of receipt ask the journal differently: a closed one is found
// by id, an open one is not in the journal yet. Overloads let std::visit treat
// them alike.
Cargo unallocated(const Journal& journal, const OpenReceipt& receipt, std::string_view cropCode) {
    return journal.unallocatedOf(receipt, cropCode);
}
Cargo unallocated(const Journal& journal, const Receipt& receipt, std::string_view cropCode) {
    return journal.unallocatedOf(receipt.id(), cropCode);
}
std::vector<std::string> check(const Journal& journal, const Allocation& allocation, const OpenReceipt& receipt) {
    return journal.validateAllocation(allocation, receipt);
}
std::vector<std::string> check(const Journal& journal, const Allocation& allocation, const Receipt&) {
    return journal.validateAllocation(allocation);
}

// Crops of the lines that count, in the order they were weighed.
template <WeighedReceipt R>
std::vector<std::string> cropsOf(const R& receipt) {
    std::vector<std::string> crops;
    for (const Weighing& w : receipt.weighings()) {
        if (!w.cancelled && std::ranges::find(crops, w.cropCode) == crops.end()) crops.push_back(w.cropCode);
    }
    return crops;
}

template <WeighedReceipt R>
Cargo unallocatedTotal(const Journal& journal, const R& receipt) {
    Cargo left;
    for (const std::string& crop : cropsOf(receipt)) left += unallocated(journal, receipt, crop);
    return left;
}

constexpr auto kWarehouseId = "warehouse";

// Step of the simulated scales; real ones are set to 200 or 500 g.
constexpr Weight kSimulatedDivision = grams(200);

// What the scale shows for `grams`: industrial scales only display whole steps.
Weight onScale(std::int64_t grams) {
    return kSimulatedDivision * divideRounded(grams, kSimulatedDivision.raw());
}

// A pallet being put down: the scale swings, then settles on `target`.
std::vector<Weight> palletLanding(Weight target) {
    const auto g = target.raw();
    return {onScale(g / 5), onScale(g * 4 / 5), onScale(g + 3'000), onScale(g - 800), onScale(g + 200), target};
}

} // namespace

Terminal::Terminal(QObject* parent)
    : QObject{parent},
      operators_{demoOperators()},
      suppliers_{demoSuppliers()},
      previousTrucks_{demoPreviousTrucks()},
      supplierModel_{suppliers_, journal_},
      simulator_{{grams(0)}},
      scaleMonitor_{simulator_} {
    addDemoHistory(journal_, previousTrucks_[1].id());
    connect(this, &Terminal::journalChanged, &supplierModel_, &SupplierListModel::refreshBalances);
    connect(this, &Terminal::journalChanged, this, &Terminal::trucksChanged);
    connect(this, &Terminal::sessionChanged, this, &Terminal::trucksChanged);
    connect(this, &Terminal::sessionChanged, this, &Terminal::pricesChanged);
    connect(this, &Terminal::sessionChanged, this, &Terminal::receiptsChanged);
    // Journal and session changes already reach trucksChanged.
    connect(this, &Terminal::trucksChanged, this, &Terminal::allocationChanged);
    connect(this, &Terminal::receiptsChanged, this, &Terminal::allocationChanged);
    connect(this, &Terminal::journalChanged, this, &Terminal::cashChanged);
}

int Terminal::journalSize() const noexcept { return static_cast<int>(journal_.size()); }

QString Terminal::supplierName() const {
    const Supplier* supplier = currentSupplier();
    return supplier ? QString::fromStdString(supplier->fullName()) : QString{};
}

QString Terminal::supplierBalance() const {
    const Supplier* supplier = currentSupplier();
    return supplier ? QString("%1 грн").arg(hryvniasText(journal_.balanceOf(supplier->id()))) : QString{};
}

QString Terminal::sessionTitle() const {
    if (!workDay_) return {};
    return QString::fromStdString(std::format("Сесія {:%d.%m.%Y}", workDay_->date()));
}

QString Terminal::sessionOpenedBy() const {
    if (!workDay_) return {};
    return tr("Відкрито: %1, %2").arg(operatorNameOf(workDay_->openedBy()), localTime(workDay_->openedAt()));
}

QVariantList Terminal::cropList() const {
    QVariantList list;
    for (const Crop& crop : kCrops)
        list.append(QVariantMap{{"cropCode", QString::fromUtf8(crop.code)}, {"name", QString::fromUtf8(crop.name)}});
    return list;
}

QVariantMap Terminal::priceMap() const {
    QVariantMap map;
    if (!workDay_) return map;
    for (const Crop& crop : kCrops) {
        if (const auto price = workDay_->prices().current(crop.code))
            map.insert(QString::fromUtf8(crop.code), hryvniasText(*price));
    }
    return map;
}

QVariantList Terminal::truckList() const {
    QVariantList list;
    if (workDay_) {
        for (const Truck& truck : workDay_->trucks()) list.append(truckInfo(truck));
    }
    return list;
}

QVariantList Terminal::previousTruckList() const {
    QVariantList list;
    for (int i = 0; i < static_cast<int>(previousTrucks_.size()); ++i) {
        const Truck& truck = previousTrucks_[static_cast<std::size_t>(i)];
        // A carried-over truck is already in the session under its own id.
        if (truck.status() == TruckStatus::CarriedOver) continue;
        list.append(QVariantMap{
            {"index", i},
            {"plate", QString::fromStdString(truck.plate())},
            {"make", QString::fromStdString(truck.details().make)},
            {"note", QString::fromStdString(truck.details().note)},
            {"capacity", describeCargo(truck.capacity())},
        });
    }
    return list;
}

QStringList Terminal::closeBlockers() const {
    QStringList list;
    if (workDay_) {
        for (const std::string& error : workDay_->validateClose()) list << QString::fromStdString(error);
    }
    return list;
}

QVariantList Terminal::openReceiptList() const {
    QVariantList list;
    if (!workDay_) return list;
    for (const OpenReceipt& receipt : workDay_->openReceipts()) {
        list.append(QVariantMap{
            {"receiptId", QString::fromStdString(receipt.id().toString())},
            {"supplierName", supplierNameOf(receipt.supplierId())},
            {"openedAt", localTime(receipt.openedAt())},
            {"boxes", receipt.total().boxes},
            {"amount", hryvniasText(amountOf(receipt))},
        });
    }
    return list;
}

QString Terminal::activeReceiptId() const {
    return activeReceiptId_ ? QString::fromStdString(activeReceiptId_->toString()) : QString{};
}

QVariantList Terminal::activeLines() const {
    QVariantList list;
    const OpenReceipt* receipt = activeReceipt();
    if (!receipt) return list;
    for (std::size_t i = 0; i < receipt->weighings().size(); ++i) {
        const Weighing& w = receipt->weighings()[i];
        list.append(QVariantMap{
            {"index", static_cast<int>(i)},
            {"crop", cropName(w.cropCode)},
            {"boxes", w.boxCount},
            {"gross", kilogramsText(w.gross)},
            {"tare", kilogramsText(w.tare())},
            {"net", kilogramsText(w.net())},
            {"price", w.pricePerKg.isZero() ? QString("—") : hryvniasText(w.pricePerKg)},
            {"amount", hryvniasText(w.amount())},
            {"manualPrice", w.priceSource == PriceSource::Manual},
            {"manualWeight", w.weightSource == WeightSource::Manual},
            {"cancelled", w.cancelled},
        });
    }
    return list;
}

QVariantMap Terminal::activeSummary() const {
    const OpenReceipt* receipt = activeReceipt();
    if (!receipt) return {};
    const Cargo total = receipt->total();
    return {
        {"openedAt", localTime(receipt->openedAt())},
        {"boxes", total.boxes},
        {"net", kilogramsText(total.weight)},
        {"amount", hryvniasText(amountOf(*receipt))},
    };
}

QVariantList Terminal::allocationSources() const {
    QVariantList list;
    const auto add = [&](const auto& receipt, TimePoint openedAt, bool open) {
        const Cargo left = unallocatedTotal(journal_, receipt);
        if (left.boxes <= 0) return;
        list.append(QVariantMap{
            {"receiptId", QString::fromStdString(receipt.id().toString())},
            {"supplierName", supplierNameOf(receipt.supplierId())},
            {"openedAt", localTime(openedAt)},
            {"open", open},
            {"left", describeCargo(left)},
        });
    };
    if (workDay_) {
        for (const OpenReceipt& receipt : workDay_->openReceipts()) add(receipt, receipt.openedAt(), true);
    }
    for (const Receipt* receipt : journal_.waitingForAllocation()) add(*receipt, receipt->createdAt(), false);
    return list;
}

QString Terminal::allocationReceiptId() const {
    return allocationReceiptId_ ? QString::fromStdString(allocationReceiptId_->toString()) : QString{};
}

QString Terminal::allocationSupplierName() const {
    const auto source = allocationReceiptId_ ? findSource(*allocationReceiptId_) : std::nullopt;
    if (!source) return {};
    return std::visit([&](const auto* receipt) { return supplierNameOf(receipt->supplierId()); }, *source);
}

QVariantList Terminal::allocationLines() const {
    QVariantList list;
    const auto source = allocationReceiptId_ ? findSource(*allocationReceiptId_) : std::nullopt;
    if (!source) return list;
    std::visit(
        [&](const auto* receipt) {
            for (std::size_t i = 0; i < receipt->weighings().size(); ++i) {
                const Weighing& w = receipt->weighings()[i];
                QString placedIn;
                if (const auto destination = journal_.destinationOf(receipt->id(), i)) {
                    const auto* truckId = std::get_if<TruckId>(&*destination);
                    placedIn = truckId ? tr("у фурі %1").arg(plateOf(*truckId)) : tr("на складі");
                }
                list.append(QVariantMap{
                    {"index", static_cast<int>(i)},
                    {"crop", cropName(w.cropCode)},
                    {"boxes", w.boxCount},
                    {"net", weight(w.net())},
                    {"cancelled", w.cancelled},
                    {"placedIn", placedIn},
                });
            }
        },
        *source);
    return list;
}

QVariantList Terminal::allocationCrops() const {
    QVariantList list;
    const auto source = allocationReceiptId_ ? findSource(*allocationReceiptId_) : std::nullopt;
    if (!source) return list;
    std::visit(
        [&](const auto* receipt) {
            for (const std::string& crop : cropsOf(*receipt)) {
                const Cargo left = unallocated(journal_, *receipt, crop);
                if (left.boxes <= 0) continue;
                list.append(QVariantMap{
                    {"cropCode", QString::fromStdString(crop)},
                    {"name", cropName(crop)},
                    {"leftBoxes", left.boxes},
                    {"left", describeCargo(left)},
                });
            }
        },
        *source);
    return list;
}

QVariantList Terminal::destinationList() const {
    QVariantList list;
    if (!workDay_) return list;
    for (const Truck& truck : workDay_->trucks()) {
        if (!truck.acceptsCargo()) continue;
        const QVariantMap info = truckInfo(truck);
        const QString make = info["make"].toString();
        list.append(QVariantMap{
            {"destinationId", info["truckId"]},
            {"title", make.isEmpty() ? info["plate"].toString() : QString("%1 · %2").arg(info["plate"].toString(), make)},
            {"detail", tr("%1 з %2").arg(info["load"].toString(), info["capacity"].toString())},
            {"fillPercent", info["fillPercent"]},
            {"overloaded", info["overloaded"]},
            {"warehouse", false},
        });
    }
    QStringList stock;
    for (const Crop& crop : kCrops) {
        if (const Cargo held = journal_.stockOf(crop.code); held.boxes != 0)
            stock << QString("%1 %2").arg(QString::fromUtf8(crop.name), describeCargo(held));
    }
    list.append(QVariantMap{
        {"destinationId", QString(kWarehouseId)},
        {"title", tr("Склад")},
        {"detail", stock.isEmpty() ? tr("порожньо") : stock.join(", ")},
        {"fillPercent", 0},
        {"overloaded", false},
        {"warehouse", true},
    });
    return list;
}

QVariantList Terminal::stockList() const {
    QVariantList list;
    for (const Crop& crop : kCrops) {
        const Cargo held = journal_.stockOf(crop.code);
        list.append(QVariantMap{
            {"cropCode", QString::fromUtf8(crop.code)},
            {"name", QString::fromUtf8(crop.name)},
            {"boxes", held.boxes},
            {"stock", held.boxes == 0 && held.weight.isZero() ? tr("порожньо") : describeCargo(held)},
            {"negative", held.boxes < 0},
        });
    }
    return list;
}

QVariantList Terminal::debtorList() const {
    std::vector<std::pair<Money, const Supplier*>> nonZero;
    for (const Supplier& supplier : suppliers_) {
        if (const Money balance = journal_.balanceOf(supplier.id()); !balance.isZero())
            nonZero.emplace_back(balance, &supplier);
    }
    std::ranges::sort(nonZero, std::ranges::greater{}, &std::pair<Money, const Supplier*>::first);

    QVariantList list;
    for (const auto& [balance, supplier] : nonZero) {
        list.append(QVariantMap{
            {"supplierId", qint64{supplier->id()}},
            {"name", QString::fromStdString(supplier->fullName())},
            {"balance", money(balance)},
            {"overpaid", balance.isNegative()},
        });
    }
    return list;
}

QString Terminal::cashSupplierName() const {
    return cashSupplierId_ ? supplierNameOf(*cashSupplierId_) : QString{};
}

QString Terminal::cashBalance() const {
    return cashSupplierId_ ? money(journal_.balanceOf(*cashSupplierId_)) : QString{};
}

QString Terminal::cashDebtInput() const {
    if (!cashSupplierId_) return {};
    const Money balance = journal_.balanceOf(*cashSupplierId_);
    return balance.isPositive() ? moneyInput(balance) : QString{};
}

QVariantList Terminal::cashHistory() const {
    QVariantList list;
    if (!cashSupplierId_) return list;
    for (const Transaction* record : journal_.historyOf(*cashSupplierId_)) {
        // describe() is virtual: each record type tells its own story. The first
        // line is the headline, the rest (receipt lines) is the detail.
        const QString text = QString::fromStdString(record->describe());
        const qsizetype newline = text.indexOf('\n');
        const Money effect = record->balanceEffect();
        list.append(QVariantMap{
            {"at", localDateTime(record->createdAt())},
            {"title", text.left(newline)},
            {"detail", newline < 0 ? QString{} : text.mid(newline + 1)},
            {"effect", effect.isNegative() ? money(effect) : "+" + money(effect)},
            {"payment", record->kind() == TransactionKind::Payment},
        });
    }
    return list;
}

QVariantList Terminal::operatorList() const {
    QVariantList list;
    for (const Operator& op : operators_)
        list.append(QVariantMap{{"operatorId", qint64{op.id()}}, {"name", QString::fromStdString(op.name())}});
    return list;
}

QString Terminal::operatorName() const {
    return currentOperatorId_ ? operatorNameOf(*currentOperatorId_) : QString{};
}

bool Terminal::login(qint64 operatorId, const QString& pin) {
    const Operator* op = findOperator(operatorId);
    if (!op || !op->checkPin(pin.toStdString())) return false;
    currentOperatorId_ = op->id();
    emit operatorChanged();
    return true;
}

void Terminal::logout() {
    if (!currentOperatorId_) return;
    setActiveReceipt(std::nullopt);
    currentOperatorId_.reset();
    emit operatorChanged();
}

void Terminal::openSession() {
    if (!currentOperatorId_ || workDay_) return;
    // The session is named by the local date; the core leaves time zones to the UI.
    const QDate today = QDate::currentDate();
    workDay_.emplace(std::chrono::year_month_day{std::chrono::year{today.year()},
                                                 std::chrono::month{static_cast<unsigned>(today.month())},
                                                 std::chrono::day{static_cast<unsigned>(today.day())}},
                     Clock::now(), *currentOperatorId_);
    for (const Truck& truck : previousTrucks_) {
        if (truck.status() == TruckStatus::CarriedOver) workDay_->adoptTruck(truck);
    }
    emit sessionChanged();
}

QString Terminal::setPrice(const QString& cropCode, const QString& price) {
    if (!workDay_) return tr("Сесію не відкрито");
    const auto money = parseMoney(price.toStdString());
    if (!money || money->isZero()) return tr("Ціна — додатне число гривень, напр. 85 або 85,50");
    workDay_->setPrice(cropCode.toStdString(), *money, Clock::now());
    emit pricesChanged();
    return {};
}

QString Terminal::addTruck(const QString& plate, const QString& make, const QString& note, const QString& boxes,
                           const QString& kilogramsText) {
    if (!workDay_) return tr("Сесію не відкрито");
    if (plate.trimmed().isEmpty()) return tr("Вкажіть номер фури");
    const auto boxLimit = parseLimit(boxes);
    const auto weightLimit = parseLimit(kilogramsText);
    if (!boxLimit || !weightLimit) return tr("Ліміт — ціле невід'ємне число");
    if (*boxLimit == 0 && *weightLimit == 0) return tr("Вкажіть ліміт фури: ящики, кілограми або обидва");
    workDay_->addTruck(plate.trimmed().toUpper().toStdString(),
                       {.boxes = static_cast<int>(*boxLimit), .weight = kilograms(*weightLimit)},
                       {.make = make.trimmed().toStdString(), .note = note.trimmed().toStdString()});
    emit trucksChanged();
    return {};
}

QString Terminal::repeatTruck(int previousIndex) {
    if (!workDay_) return tr("Сесію не відкрито");
    if (previousIndex < 0 || previousIndex >= static_cast<int>(previousTrucks_.size())) return tr("Оберіть фуру");
    workDay_->repeatTruck(previousTrucks_[static_cast<std::size_t>(previousIndex)]);
    emit trucksChanged();
    return {};
}

void Terminal::departTruck(const QString& truckId) {
    const auto id = Uuid::parse(truckId.toStdString());
    if (!workDay_ || !id) return;
    workDay_->departTruck(*id);
    emit trucksChanged();
}

void Terminal::carryOverTruck(const QString& truckId) {
    const auto id = Uuid::parse(truckId.toStdString());
    if (!workDay_ || !id) return;
    workDay_->carryOverTruck(*id);
    emit trucksChanged();
}

void Terminal::closeSession() {
    if (!workDay_ || !currentOperatorId_ || !workDay_->validateClose().empty()) return;
    workDay_->close(Clock::now(), *currentOperatorId_);
    // The closed session itself is dropped until SQLite keeps the archive.
    previousTrucks_ = workDay_->trucks();
    workDay_.reset();
    emit sessionChanged();
}

QString Terminal::startReceipt(qint64 supplierId) {
    if (!workDay_) return tr("Сесію не відкрито");
    if (std::ranges::find(suppliers_, supplierId, &Supplier::id) == suppliers_.end())
        return tr("Постачальника #%1 немає").arg(supplierId);
    const OpenReceipt& receipt = workDay_->openReceipt(supplierId);
    setActiveReceipt(receipt.id());
    return {};
}

void Terminal::activateReceipt(const QString& receiptId) {
    const auto id = Uuid::parse(receiptId.toStdString());
    if (workDay_ && id && workDay_->findReceipt(*id)) setActiveReceipt(*id);
}

QString Terminal::recordWeighing(const QString& cropCode, const QString& boxes, const QString& tarePerBox,
                                 const QString& palletTare, const QString& price, const QString& manualWeight,
                                 bool acceptUnstable) {
    if (!workDay_ || !activeReceipt()) return tr("Спершу відкрийте чек постачальника");

    Weighing weighing;
    weighing.cropCode = cropCode.toStdString();
    bool ok = false;
    weighing.boxCount = boxes.trimmed().toInt(&ok);
    if (!ok) return tr("Кількість ящиків — ціле число");
    const auto boxTare = parseWeight(tarePerBox.toStdString());
    if (!boxTare) return tr("Тара ящика — кілограми, напр. 0,35");
    weighing.tarePerBox = *boxTare;
    // Asked every time on purpose: pallets differ, and 0 is a real answer.
    const auto pallet = parseWeight(palletTare.toStdString());
    if (!pallet) return tr("Вкажіть вагу палети, кг (0 — без палети)");
    weighing.palletTare = *pallet;

    if (!price.trimmed().isEmpty()) {
        const auto manual = parseMoney(price.toStdString());
        if (!manual || manual->isZero()) return tr("Ціна — додатне число гривень, напр. 85 або 85,50");
        weighing.pricePerKg = *manual;
        weighing.priceSource = PriceSource::Manual;
    }

    if (!manualWeight.trimmed().isEmpty()) {
        const auto typed = parseWeight(manualWeight.toStdString());
        if (!typed) return tr("Вага брутто — кілограми, напр. 185,4");
        weighing.gross = *typed;
        weighing.weightSource = WeightSource::Manual;
    } else if (const auto stable = scaleMonitor_.stableWeight()) {
        weighing.gross = *stable;
    } else if (const auto reading = scaleMonitor_.lastReading(); acceptUnstable && reading) {
        // The scale steps by 200-500 g, so flipping between two steps is within that error.
        weighing.gross = *reading;
    } else {
        return tr("Вага ще не стабільна: зачекайте або підтвердіть нестабільну");
    }

    if (const auto errors = weighing.validate(); !errors.empty()) return joined(errors);
    workDay_->addWeighing(*activeReceiptId_, std::move(weighing), Clock::now());
    emit receiptsChanged();
    return {};
}

QString Terminal::cancelWeighing(int index) {
    const OpenReceipt* receipt = activeReceipt();
    if (!receipt || index < 0 || index >= static_cast<int>(receipt->weighings().size())) return tr("Немає такого рядка");
    const auto i = static_cast<std::size_t>(index);
    if (const auto errors = journal_.validateCancel(*receipt, i); !errors.empty()) return joined(errors);
    workDay_->cancelWeighing(*activeReceiptId_, i, journal_);
    emit receiptsChanged();
    return {};
}

QStringList Terminal::receiptCloseBlockers(bool wholesale) const {
    QStringList list;
    const OpenReceipt* receipt = activeReceipt();
    if (!receipt) return list;
    for (const std::string& error : receipt->validateClose(wholesale ? Settlement::HandedOver : Settlement::OnSite))
        list << QString::fromStdString(error);
    return list;
}

void Terminal::closeReceipt(bool wholesale) {
    if (!activeReceipt() || !receiptCloseBlockers(wholesale).isEmpty()) return;
    journal_.add(workDay_->closeReceipt(*activeReceiptId_, wholesale ? Settlement::HandedOver : Settlement::OnSite));
    const auto& left = workDay_->openReceipts();
    setActiveReceipt(left.empty() ? std::nullopt : std::optional{left.front().id()});
    emit journalChanged();
    emit balanceChanged();
}

void Terminal::selectAllocationReceipt(const QString& receiptId) {
    const auto id = Uuid::parse(receiptId.toStdString());
    if (!id || !findSource(*id)) return;
    allocationReceiptId_ = *id;
    emit allocationChanged();
}

QString Terminal::allocateWeighing(int index, const QString& destinationId) {
    const auto source = allocationReceiptId_ ? findSource(*allocationReceiptId_) : std::nullopt;
    if (!source) return tr("Оберіть чек");
    const auto destination = destinationFor(destinationId);
    if (const auto* error = std::get_if<QString>(&destination)) return *error;

    return std::visit(
        [&](const auto* receipt) {
            if (index < 0 || index >= static_cast<int>(receipt->weighings().size())) return tr("Немає такого рядка");
            return allocate(*source, Allocation::wholeWeighing(*receipt, static_cast<std::size_t>(index),
                                                               std::get<Destination>(destination)));
        },
        *source);
}

QString Terminal::allocateBoxes(const QString& cropCode, const QString& boxesText, const QString& destinationId) {
    const auto source = allocationReceiptId_ ? findSource(*allocationReceiptId_) : std::nullopt;
    if (!source) return tr("Оберіть чек");
    bool ok = false;
    const int boxes = boxesText.trimmed().toInt(&ok);
    if (!ok || boxes <= 0) return tr("Кількість ящиків — ціле додатне число");
    const auto destination = destinationFor(destinationId);
    if (const auto* error = std::get_if<QString>(&destination)) return *error;

    const std::string crop = cropCode.toStdString();
    return std::visit(
        [&](const auto* receipt) {
            // Allocation::boxesOf() throws for a crop the receipt does not have.
            if (receipt->cargoOf(crop).boxes <= 0) return tr("У чеку немає цієї культури");
            return allocate(*source, Allocation::boxesOf(*receipt, crop, boxes, std::get<Destination>(destination),
                                                         unallocated(journal_, *receipt, crop)));
        },
        *source);
}

QString Terminal::shipFromStock(const QString& cropCode, const QString& boxesText, const QString& truckId) {
    bool ok = false;
    const int boxes = boxesText.trimmed().toInt(&ok);
    if (!ok || boxes <= 0) return tr("Кількість ящиків — ціле додатне число");
    // From the warehouse only into a truck: "warehouse to warehouse" makes no sense.
    if (truckId == kWarehouseId) return tr("Оберіть фуру");
    const auto destination = destinationFor(truckId);
    if (const auto* error = std::get_if<QString>(&destination)) return *error;

    const StockShipment shipment =
        journal_.shipmentOf(cropCode.toStdString(), boxes, std::get<TruckId>(std::get<Destination>(destination)));
    if (const auto errors = journal_.validateShipment(shipment); !errors.empty()) return joined(errors);
    journal_.add(shipment);
    emit journalChanged();
    return {};
}

void Terminal::selectCashSupplier(qint64 supplierId) {
    const auto found = std::ranges::find(suppliers_, supplierId, &Supplier::id);
    cashSupplierId_ = found != suppliers_.end() ? std::optional<SupplierId>{found->id()} : std::nullopt;
    emit cashChanged();
}

QString Terminal::balanceAfter(const QString& amount) const {
    const auto paid = parseMoney(amount.toStdString());
    if (!cashSupplierId_ || !paid) return {};
    return money(journal_.balanceOf(*cashSupplierId_) - *paid);
}

QString Terminal::pay(const QString& amount, bool bankTransfer) {
    if (!cashSupplierId_) return tr("Оберіть постачальника");
    const auto paid = parseMoney(amount.toStdString());
    if (!paid) return tr("Сума — число гривень, напр. 1500 або 1500,50");
    const Payment payment = Payment::create(*cashSupplierId_, *paid,
                                            bankTransfer ? PaymentMethod::BankTransfer : PaymentMethod::Cash);
    if (const auto errors = payment.validate(); !errors.empty()) return joined(errors);
    journal_.add(payment);
    emit journalChanged();
    emit balanceChanged();
    return {};
}

void Terminal::simulatePallet() {
    // 150-260 kg, a typical pallet of berries on platform scales.
    const auto kg = QRandomGenerator::global()->bounded(150, 260);
    simulator_.setScript(palletLanding(onScale(kg * 1'000 + QRandomGenerator::global()->bounded(1'000))));
}

void Terminal::simulateEmptyScale() { simulator_.setScript({grams(0)}); }

const Supplier* Terminal::currentSupplier() const noexcept {
    const OpenReceipt* receipt = activeReceipt();
    if (!receipt) return nullptr;
    const auto found = std::ranges::find(suppliers_, receipt->supplierId(), &Supplier::id);
    return found != suppliers_.end() ? &*found : nullptr;
}

const Operator* Terminal::findOperator(OperatorId id) const noexcept {
    const auto found = std::ranges::find(operators_, id, &Operator::id);
    return found != operators_.end() ? &*found : nullptr;
}

QString Terminal::operatorNameOf(OperatorId id) const {
    const Operator* op = findOperator(id);
    return op ? QString::fromStdString(op->name()) : QString("#%1").arg(id);
}

QVariantMap Terminal::truckInfo(const Truck& truck) const {
    const Cargo load = journal_.loadOf(truck.id());
    return {
        {"truckId", QString::fromStdString(truck.id().toString())},
        {"plate", QString::fromStdString(truck.plate())},
        {"make", QString::fromStdString(truck.details().make)},
        {"note", QString::fromStdString(truck.details().note)},
        {"capacity", describeCargo(truck.capacity())},
        {"load", describeCargo(load)},
        // Bare numbers for the session table; an empty limit means "no limit".
        {"loadBoxes", QString::number(load.boxes)},
        {"loadKg", kilogramsText(load.weight)},
        {"limitBoxes", truck.capacity().boxes != 0 ? QString::number(truck.capacity().boxes) : QString{}},
        {"limitKg", truck.capacity().weight.isZero() ? QString{} : kilogramsText(truck.capacity().weight)},
        {"fillPercent", truck.fillPercent(load)},
        {"overloaded", truck.isOverloaded(load)},
        {"status", statusText(truck.status())},
        {"loading", truck.acceptsCargo()},
    };
}

const OpenReceipt* Terminal::activeReceipt() const noexcept {
    return workDay_ && activeReceiptId_ ? workDay_->findReceipt(*activeReceiptId_) : nullptr;
}

std::optional<Terminal::SourceReceipt> Terminal::findSource(const Uuid& id) const noexcept {
    if (workDay_) {
        if (const OpenReceipt* open = workDay_->findReceipt(id)) return open;
    }
    if (const Receipt* closed = journal_.findReceipt(id)) return closed;
    return std::nullopt;
}

QString Terminal::supplierNameOf(SupplierId id) const {
    const auto found = std::ranges::find(suppliers_, id, &Supplier::id);
    return found != suppliers_.end() ? QString::fromStdString(found->fullName()) : QString("#%1").arg(id);
}

QString Terminal::plateOf(const TruckId& id) const {
    if (workDay_) {
        if (const Truck* truck = workDay_->findTruck(id)) return QString::fromStdString(truck->plate());
    }
    const auto found = std::ranges::find(previousTrucks_, id, &Truck::id);
    return found != previousTrucks_.end() ? QString::fromStdString(found->plate()) : tr("(невідома)");
}

std::variant<Destination, QString> Terminal::destinationFor(const QString& destinationId) const {
    if (!workDay_) return tr("Сесію не відкрито");
    if (destinationId == kWarehouseId) return Destination{ToWarehouse{}};
    const auto id = Uuid::parse(destinationId.toStdString());
    const Truck* truck = id ? workDay_->findTruck(*id) : nullptr;
    if (!truck) return tr("Оберіть фуру або склад");
    if (!truck->acceptsCargo()) return tr("Фура %1 вже не завантажується").arg(QString::fromStdString(truck->plate()));
    return Destination{truck->id()};
}

QString Terminal::allocate(const SourceReceipt& source, const Allocation& allocation) {
    const auto errors = std::visit([&](const auto* receipt) { return check(journal_, allocation, *receipt); }, source);
    if (!errors.empty()) return joined(errors);
    journal_.add(allocation);
    emit journalChanged();
    return {};
}

// Emits always: even for the same id its lines may have changed.
void Terminal::setActiveReceipt(std::optional<Uuid> id) {
    activeReceiptId_ = std::move(id);
    emit receiptsChanged();
    emit currentSupplierChanged();
    emit balanceChanged();
}

} // namespace wms::ui
