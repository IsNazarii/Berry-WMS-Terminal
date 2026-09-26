#include "terminal.h"

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

QString joined(const std::vector<std::string>& errors) {
    QStringList list;
    for (const std::string& error : errors) list << QString::fromStdString(error);
    return list.join("\n");
}

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
}

int Terminal::journalSize() const noexcept { return static_cast<int>(journal_.size()); }

QString Terminal::supplierName() const {
    const Supplier* supplier = currentSupplier();
    return supplier ? QString::fromStdString(supplier->fullName()) : QString{};
}

QString Terminal::supplierBalance() const {
    const Supplier* supplier = currentSupplier();
    return supplier ? QString::fromStdString(formatMoney(journal_.balanceOf(supplier->id()))) : QString{};
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
            map.insert(QString::fromUtf8(crop.code), QString::fromStdString(formatMoney(*price)));
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
        const auto supplier = std::ranges::find(suppliers_, receipt.supplierId(), &Supplier::id);
        list.append(QVariantMap{
            {"receiptId", QString::fromStdString(receipt.id().toString())},
            {"supplierName", supplier != suppliers_.end() ? QString::fromStdString(supplier->fullName())
                                                          : QString("#%1").arg(receipt.supplierId())},
            {"openedAt", localTime(receipt.openedAt())},
            {"total", describeCargo(receipt.total())},
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
            {"gross", weight(w.gross)},
            {"tare", weight(w.tare())},
            {"net", weight(w.net())},
            {"price", w.pricePerKg.isZero() ? QString("—") : money(w.pricePerKg)},
            {"amount", money(w.amount())},
            {"manualPrice", w.priceSource == PriceSource::Manual},
            {"manualWeight", w.weightSource == WeightSource::Manual},
            {"cancelled", w.cancelled},
        });
    }
    return list;
}

QString Terminal::activeTotal() const {
    const OpenReceipt* receipt = activeReceipt();
    if (!receipt) return {};
    return QString("%1 · %2").arg(describeCargo(receipt->total()), money(amountOf(*receipt)));
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
        {"fillPercent", truck.fillPercent(load)},
        {"overloaded", truck.isOverloaded(load)},
        {"status", statusText(truck.status())},
        {"loading", truck.acceptsCargo()},
    };
}

const OpenReceipt* Terminal::activeReceipt() const noexcept {
    return workDay_ && activeReceiptId_ ? workDay_->findReceipt(*activeReceiptId_) : nullptr;
}

// Emits always: even for the same id its lines may have changed.
void Terminal::setActiveReceipt(std::optional<Uuid> id) {
    activeReceiptId_ = std::move(id);
    emit receiptsChanged();
    emit currentSupplierChanged();
    emit balanceChanged();
}

} // namespace wms::ui
