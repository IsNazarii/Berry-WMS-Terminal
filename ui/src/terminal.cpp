#include "terminal.h"

#include "display_format.h"

#include "wms/allocation.h"
#include "wms/backup.h"
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/text.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QtLogging>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>
#include <exception>
#include <format>

namespace wms::ui {
namespace {

// Reference data the server will send later; until then an empty table gets
// a demo list, so a terminal is usable on the first start.
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
        {77, "ФГ «Ягідний край»"},
    };
}

template <class T>
std::vector<T> loadOrSeed(sqlite::Database& db, Repository<T>& store, const std::vector<T>& demo) {
    if (std::vector<T> records = store.all(); !records.empty()) return records;
    sqlite::ScopedTransaction transaction{db};
    for (const T& record : demo) store.save(record);
    transaction.commit();
    return store.all();
}

// Five keys and the "Ще" menu fill one row on a 1024 px screen.
constexpr std::size_t kCropKeys = 5;

QStringList toQStringList(const std::vector<std::string>& codes) {
    QStringList list;
    for (const std::string& code : codes) list << QString::fromStdString(code);
    return list;
}

std::vector<std::string> toCodes(const QStringList& list) {
    std::vector<std::string> codes;
    for (const QString& code : list) codes.push_back(code.toStdString());
    return codes;
}

// Archived crops stay in the list for their names, but the operator no longer picks them.
std::vector<std::string> activeCodesOf(const std::vector<Crop>& crops) {
    std::vector<std::string> codes;
    for (const Crop& crop : crops) {
        if (!crop.archived()) codes.push_back(crop.code());
    }
    return codes;
}

// A layout of this terminal's screen, so settings rather than the database.
CropKeys loadCropKeys(const std::vector<Crop>& crops) {
    const QSettings settings;
    CropKeys keys{kCropKeys, toCodes(settings.value("ui/cropKeys").toStringList()),
                  toCodes(settings.value("ui/cropsRecent").toStringList())};
    keys.fill(activeCodesOf(crops));
    return keys;
}

// The usual crops of the point; the operator adds the rest during the season.
std::vector<Crop> demoCrops() {
    return {
        {"raspberry", "Малина"},
        {"strawberry", "Полуниця"},
        {"blueberry", "Лохина"},
    };
}

// The session is named by the local date; the core leaves time zones to the UI.
std::chrono::year_month_day toDate(const QDate& date) {
    return std::chrono::year{date.year()} / std::chrono::month{static_cast<unsigned>(date.month())} /
           std::chrono::day{static_cast<unsigned>(date.day())};
}

// The terminal's own database in the per-user data folder; the WMS_DATABASE
// variable points elsewhere, e.g. ":memory:" for a run that leaves no trace.
std::string databasePath() {
    if (qEnvironmentVariableIsSet("WMS_DATABASE")) return qEnvironmentVariable("WMS_DATABASE").toStdString();
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir{}.mkpath(folder);
    return QDir{folder}.filePath("terminal.db").toStdString();
}

// Copies sit next to the database. A day is one session, so 30 copies cover
// about a month: enough to notice a mistake and go back to before it.
constexpr std::size_t kBackupsKept = 30;

std::optional<std::filesystem::path> backupFolderFor(const std::string& database) {
    if (database == ":memory:") return std::nullopt;
    return std::filesystem::path{database}.parent_path() / "backups";
}

// Yesterday's session: one truck left, one was carried over half-loaded.
WorkDay demoYesterday(OperatorId by) {
    const TimePoint noon = Clock::now() - std::chrono::hours{24};
    WorkDay day{toDate(QDate::currentDate().addDays(-1)), noon, by};
    day.departTruck(day.addTruck("BI1234AA", 1'200).id());
    day.carryOverTruck(day.addTruck("BI5678BB", 2'000).id());
    day.close(noon + std::chrono::hours{8}, by);
    return day;
}

// History from an earlier day, so the cash screen has a debt to show and the
// carried-over truck already has a pallet in it.
void addDemoHistory(JournalStore& journal, const TruckId& carriedOverTruck, OperatorId by) {
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {{
        .cropCode = "raspberry",
        .gross = kilograms(120),
        .boxCount = 40,
        .tarePerBox = grams(350),
        .palletTare = kilograms(25),
        .pricePerKg = hryvnias(85),
    }});
    journal.save(receipt);
    journal.save(Allocation::wholeWeighing(receipt, 0, carriedOverTruck));
    journal.save(Payment::create(42, hryvnias(3'000), PaymentMethod::Cash, by));

    // Strawberries that waited overnight in the warehouse: stock outlives the session.
    const Receipt stored = Receipt::create(45, Settlement::OnSite, {{
        .cropCode = "strawberry",
        .gross = kilograms(90),
        .boxCount = 30,
        .tarePerBox = grams(300),
        .palletTare = kilograms(20),
        .pricePerKg = hryvnias(60),
    }});
    journal.save(stored);
    journal.save(Allocation::wholeWeighing(stored, 0, ToWarehouse{}));
}

QString localTime(TimePoint at) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(at.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms).toString("HH:mm");
}

// "340 ящ. · 120.000 кг"; a zero part is left out, since for a limit it means "no limit".
QString describeCargo(const Cargo& cargo) {
    QStringList parts;
    if (cargo.boxes != 0) parts << QString("%1 ящ.").arg(cargo.boxes);
    if (!cargo.weight.isZero()) parts << QString("%1 кг").arg(kilogramsText(cargo.weight));
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
std::optional<int> parseBoxLimit(const QString& text) {
    bool ok = false;
    const int value = text.trimmed().toInt(&ok);
    return ok && value > 0 ? std::optional<int>{value} : std::nullopt;
}

QString describeBoxLimit(const Truck& truck) { return Terminal::tr("%1 ящ.").arg(truck.boxLimit()); }

// Money of the lines that count; the receipt's own amount() exists only once it is closed.
Money amountOf(const OpenReceipt& receipt) {
    Money sum;
    for (const Weighing& w : receipt.weighings()) {
        if (!w.cancelled) sum += w.amount();
    }
    return sum;
}

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
      databasePath_{databasePath()},
      backupFolder_{backupFolderFor(databasePath_)},
      db_{databasePath_},
      journalStore_{db_},
      sessionStore_{db_},
      operatorStore_{db_},
      supplierStore_{db_},
      cropStore_{db_},
      operators_{loadOrSeed(db_, operatorStore_, demoOperators())},
      suppliers_{loadOrSeed(db_, supplierStore_, demoSuppliers())},
      crops_{loadOrSeed(db_, cropStore_, demoCrops())},
      cropKeys_{loadCropKeys(crops_)},
      supplierModel_{suppliers_, journal_},
      simulator_{{grams(0)}},
      scaleMonitor_{simulator_},
      receivingTargetId_{QSettings{}.value("receiving/target").toString()} {
    if (!sessionStore_.current() && !sessionStore_.lastClosed() && journalStore_.load().size() == 0) seedDemoData();
    // The supplier model reads balances only when asked, so loading after it is fine.
    journal_ = journalStore_.load();
    workDay_ = sessionStore_.current();
    if (const auto closed = sessionStore_.lastClosed()) previousTrucks_ = closed->trucks();
    connect(this, &Terminal::journalChanged, &supplierModel_, &SupplierListModel::refreshBalances);
    connect(this, &Terminal::journalChanged, this, &Terminal::trucksChanged);
    connect(this, &Terminal::sessionChanged, this, &Terminal::trucksChanged);
    connect(this, &Terminal::sessionChanged, this, &Terminal::pricesChanged);
    connect(this, &Terminal::sessionChanged, this, &Terminal::receiptsChanged);
    // Journal and session changes already reach trucksChanged.
    connect(this, &Terminal::trucksChanged, this, &Terminal::allocationChanged);
    connect(this, &Terminal::receiptsChanged, this, &Terminal::allocationChanged);
    connect(this, &Terminal::journalChanged, this, &Terminal::cashChanged);
    connect(this, &Terminal::trucksChanged, this, &Terminal::receivingTargetChanged);
    connect(this, &Terminal::receivingTargetChanged, this, &Terminal::placementsChanged);
    connect(this, &Terminal::receiptsChanged, this, &Terminal::placementsChanged);
    connect(this, &Terminal::cropsChanged, this, &Terminal::placementsChanged);
    // The state as the last run left it, before anyone changes anything.
    backup();
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
    for (const Crop& crop : crops_) {
        if (crop.archived()) continue;
        list.append(QVariantMap{{"cropCode", QString::fromStdString(crop.code())},
                                {"name", QString::fromStdString(crop.name())}});
    }
    return list;
}

QStringList Terminal::cropKeyList() const {
    return toQStringList(cropKeys_.keys());
}

QVariantMap Terminal::priceMap() const {
    QVariantMap map;
    if (!workDay_) return map;
    for (const Crop& crop : crops_) {
        if (const auto price = workDay_->prices().current(crop.code()))
            map.insert(QString::fromStdString(crop.code()), hryvniasText(*price));
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
            {"capacity", describeBoxLimit(truck)},
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
        const auto placed = journal_.destinationOf(receipt->id(), i);
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
            {"placedIn", placed ? placeName(*placed) : QString{}},
            {"hanging", !w.cancelled && !placed},
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

QString Terminal::receivingTarget() const { return receivingDestination() ? receivingTargetId_ : QString{}; }

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
            {"leftBoxes", left.boxes},
            {"leftKg", kilogramsText(left.weight)},
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
                    {"net", kilogramsText(w.net())},
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
                    {"leftKg", kilogramsText(left.weight)},
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
            {"loadBoxes", info["loadBoxes"]},
            {"loadKg", info["loadKg"]},
            {"limitBoxes", info["limitBoxes"]},
            {"fillPercent", info["fillPercent"]},
            {"overloaded", info["overloaded"]},
            {"warehouse", false},
        });
    }
    QStringList stock;
    for (const Crop& crop : crops_) {
        if (const Cargo held = journal_.stockOf(crop.code()); held.boxes != 0)
            stock << QString("%1 %2").arg(QString::fromStdString(crop.name()), describeCargo(held));
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
    for (const Crop& crop : crops_) {
        const Cargo held = journal_.stockOf(crop.code());
        // Archiving waits for an empty warehouse, but a sync may still bring boxes in: those are shown.
        if (crop.archived() && held.boxes == 0 && held.weight.isZero()) continue;
        list.append(QVariantMap{
            {"cropCode", QString::fromStdString(crop.code())},
            {"name", QString::fromStdString(crop.name())},
            {"boxes", held.boxes},
            {"kg", kilogramsText(held.weight)},
            {"empty", held.boxes == 0 && held.weight.isZero()},
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
            {"balance", hryvniasText(balance)},
            {"overpaid", balance.isNegative()},
        });
    }
    return list;
}

QString Terminal::cashSupplierName() const {
    return cashSupplierId_ ? supplierNameOf(*cashSupplierId_) : QString{};
}

QString Terminal::cashBalance() const {
    return cashSupplierId_ ? hryvniasText(journal_.balanceOf(*cashSupplierId_)) : QString{};
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
        // A payment has no lines; its detail is who handed the money over.
        const auto* payment = dynamic_cast<const Payment*>(record);
        const QString detail = payment ? tr("видав(ла): %1").arg(operatorNameOf(payment->paidBy()))
                               : newline < 0 ? QString{}
                                             : text.mid(newline + 1);
        list.append(QVariantMap{
            {"at", localDateTime(record->createdAt())},
            {"title", text.left(newline)},
            {"detail", detail},
            {"effect", effect.isNegative() ? hryvniasText(effect) : "+" + hryvniasText(effect)},
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
    workDay_.emplace(toDate(QDate::currentDate()), Clock::now(), *currentOperatorId_);
    for (const Truck& truck : previousTrucks_) {
        if (truck.status() == TruckStatus::CarriedOver) workDay_->adoptTruck(truck);
    }
    saveSession();
    emit sessionChanged();
}

QString Terminal::setPrice(const QString& cropCode, const QString& price) {
    if (!workDay_) return tr("Сесію не відкрито");
    const auto money = parseMoney(price.toStdString());
    if (!money || money->isZero()) return tr("Ціна — додатне число гривень, напр. 85 або 85,50");
    workDay_->setPrice(cropCode.toStdString(), *money, Clock::now());
    saveSession();
    emit pricesChanged();
    useCrop(cropCode);
    return {};
}

QString Terminal::addTruck(const QString& plate, const QString& make, const QString& note, const QString& boxes) {
    if (!workDay_) return tr("Сесію не відкрито");
    if (plate.trimmed().isEmpty()) return tr("Вкажіть номер фури");
    const auto boxLimit = parseBoxLimit(boxes);
    if (!boxLimit) return tr("Ліміт фури — ціле додатне число ящиків");
    workDay_->addTruck(plate.trimmed().toUpper().toStdString(), *boxLimit,
                       {.make = make.trimmed().toStdString(), .note = note.trimmed().toStdString()});
    saveSession();
    emit trucksChanged();
    return {};
}

QString Terminal::repeatTruck(int previousIndex) {
    if (!workDay_) return tr("Сесію не відкрито");
    if (previousIndex < 0 || previousIndex >= static_cast<int>(previousTrucks_.size())) return tr("Оберіть фуру");
    workDay_->repeatTruck(previousTrucks_[static_cast<std::size_t>(previousIndex)]);
    saveSession();
    emit trucksChanged();
    return {};
}

void Terminal::departTruck(const QString& truckId) {
    const auto id = Uuid::parse(truckId.toStdString());
    if (!workDay_ || !id) return;
    workDay_->departTruck(*id);
    saveSession();
    emit trucksChanged();
}

void Terminal::carryOverTruck(const QString& truckId) {
    const auto id = Uuid::parse(truckId.toStdString());
    if (!workDay_ || !id) return;
    workDay_->carryOverTruck(*id);
    saveSession();
    emit trucksChanged();
}

void Terminal::closeSession() {
    if (!workDay_ || !currentOperatorId_ || !workDay_->validateClose().empty()) return;
    workDay_->close(Clock::now(), *currentOperatorId_);
    // The database keeps the closed session as the archive; here only its trucks are needed.
    saveSession();
    previousTrucks_ = workDay_->trucks();
    workDay_.reset();
    emit sessionChanged();
    // The end of the working day: a copy with the whole day in it.
    backup();
}

QStringList Terminal::newCropErrors(const QString& name) const {
    QStringList list;
    for (const std::string& error : validateNewCrop(name.toStdString(), crops_)) list << QString::fromStdString(error);
    return list;
}

QString Terminal::addCrop(const QString& name) {
    if (const QStringList errors = newCropErrors(name); !errors.isEmpty()) return errors.join("\n");
    const std::string typed = squeezeSpaces(name.toStdString());
    // Validation lets through only an archived match: that crop comes back, code and place kept.
    if (const Crop* archived = findCropNamed(typed, crops_)) {
        const Crop restored{archived->code(), typed};
        cropStore_.save(restored);
        *std::ranges::find(crops_, restored.code(), &Crop::code) = restored;
    } else {
        const Crop crop{cropCodeFor(typed), typed};
        cropStore_.save(crop);
        crops_.push_back(crop);
    }
    cropsUpdated();
    return {};
}

QVariantMap Terminal::cropRemoval(const QString& cropCode) const {
    const std::string code = cropCode.toStdString();
    QStringList errors;
    for (const std::string& error : validateRemoveCrop(code, journal_)) errors << QString::fromStdString(error);
    return {{"errors", errors}, {"keepsRecords", hasCropRecords(code)}};
}

QString Terminal::removeCrop(const QString& cropCode) {
    const std::string code = cropCode.toStdString();
    const auto crop = std::ranges::find(crops_, code, &Crop::code);
    if (crop == crops_.end() || crop->archived()) return tr("Немає такої культури");
    if (const auto errors = validateRemoveCrop(code, journal_); !errors.empty()) return joined(errors);
    if (hasCropRecords(code)) {
        *crop = Crop{crop->code(), crop->name(), true};
        cropStore_.save(*crop);
    } else {
        cropStore_.remove(*crop);
        crops_.erase(crop);
    }
    cropsUpdated();
    return {};
}

void Terminal::useCrop(const QString& cropCode) {
    if (!isActiveCrop(cropCode.toStdString())) return;
    cropKeys_.use(cropCode.toStdString());
    saveCropKeys();
    emit cropKeysChanged();
}

bool Terminal::isActiveCrop(std::string_view code) const noexcept {
    const auto crop = std::ranges::find(crops_, code, &Crop::code);
    return crop != crops_.end() && !crop->archived();
}

bool Terminal::hasCropRecords(std::string_view code) const noexcept {
    return journal_.mentionsCrop(code) || (workDay_ && workDay_->mentionsCrop(code));
}

void Terminal::saveCropKeys() const {
    QSettings settings;
    settings.setValue("ui/cropKeys", toQStringList(cropKeys_.keys()));
    settings.setValue("ui/cropsRecent", toQStringList(cropKeys_.recent()));
}

void Terminal::cropsUpdated() {
    cropKeys_.fill(activeCodesOf(crops_));
    saveCropKeys();
    emit cropsChanged();
    emit cropKeysChanged();
    // Prices and stock list crops too.
    emit pricesChanged();
    emit journalChanged();
}

QString Terminal::startReceipt(qint64 supplierId) {
    if (!workDay_) return tr("Сесію не відкрито");
    if (std::ranges::find(suppliers_, supplierId, &Supplier::id) == suppliers_.end())
        return tr("Постачальника #%1 немає").arg(supplierId);
    const Uuid id = workDay_->openReceipt(supplierId).id();
    saveSession(); // the receipt may be new
    setActiveReceipt(id);
    return {};
}

QStringList Terminal::newSupplierErrors(const QString& fullName, const QString& phone) const {
    QStringList list;
    for (const std::string& error :
         validateNewSupplier(fullName.toStdString(), squeezeSpaces(phone.toStdString()), suppliers_))
        list << QString::fromStdString(error);
    return list;
}

qint64 Terminal::addSupplier(const QString& fullName, const QString& phone) {
    if (!newSupplierErrors(fullName, phone).isEmpty()) return 0;
    const Supplier supplier{nextSupplierId(suppliers_), squeezeSpaces(fullName.toStdString()),
                            squeezeSpaces(phone.toStdString())};
    supplierStore_.save(supplier);
    suppliers_.push_back(supplier);
    supplierModel_.refresh();
    return supplier.id();
}

void Terminal::activateReceipt(const QString& receiptId) {
    const auto id = Uuid::parse(receiptId.toStdString());
    if (workDay_ && id && workDay_->findReceipt(*id)) setActiveReceipt(*id);
}

QString Terminal::recordWeighing(const QString& cropCode, const QString& boxes, const QString& tarePerBox,
                                 const QString& palletTare, const QString& price, const QString& manualWeight,
                                 bool acceptUnstable) {
    if (!workDay_ || !activeReceipt()) return tr("Спершу відкрийте чек постачальника");

    if (!isActiveCrop(cropCode.toStdString())) return tr("Оберіть культуру");
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
    const std::size_t index = workDay_->addWeighing(*activeReceiptId_, std::move(weighing), Clock::now());
    auto allocation = workDay_->autoAllocation(*activeReceiptId_, index, receivingDestination(), journal_);
    // The rule only picks the place; a pallet the journal would refuse just hangs.
    if (allocation && !journal_.validateAllocation(*allocation, *activeReceipt()).empty()) allocation.reset();
    {
        // The pallet and where it went together, or neither after a crash.
        sqlite::ScopedTransaction transaction{db_};
        saveSession();
        if (allocation) journalStore_.save(*allocation);
        transaction.commit();
    }
    if (allocation) {
        journal_.add(std::move(*allocation));
        emit journalChanged();
    }
    emit receiptsChanged();
    useCrop(cropCode);
    return {};
}

QString Terminal::cancelWeighing(int index) {
    const OpenReceipt* receipt = activeReceipt();
    if (!receipt || index < 0 || index >= static_cast<int>(receipt->weighings().size())) return tr("Немає такого рядка");
    const auto i = static_cast<std::size_t>(index);
    if (const auto errors = workDay_->validateCancel(*activeReceiptId_, i, journal_); !errors.empty())
        return joined(errors);
    const auto reversal = workDay_->cancelWeighing(*activeReceiptId_, i, journal_);
    {
        // The cancelled line and its freed allocation together, or neither after a crash.
        sqlite::ScopedTransaction transaction{db_};
        if (reversal) journalStore_.save(*reversal);
        saveSession();
        transaction.commit();
    }
    if (reversal) {
        journal_.add(*reversal);
        emit journalChanged();
    }
    emit receiptsChanged();
    return {};
}

void Terminal::setReceivingTarget(const QString& destinationId) {
    receivingTargetId_ = destinationId;
    QSettings{}.setValue("receiving/target", destinationId);
    emit receivingTargetChanged();
}

QVariantMap Terminal::nextPlacements() const {
    QVariantMap map;
    if (!workDay_ || !activeReceipt()) return map;
    for (const Crop& crop : crops_) map.insert(QString::fromStdString(crop.code()), nextPlacement(crop.code()));
    return map;
}

QVariantMap Terminal::nextPlacement(std::string_view cropCode) const {
    const auto target = receivingDestination();
    const auto next = workDay_->nextDestination(*activeReceiptId_, cropCode, target, journal_);
    if (!next) {
        return {{"text", tr("нікуди: без цілі палета чекатиме на «Розподілі»")},
                {"follows", false}, {"hangs", true}, {"boxesLeft", -1}};
    }
    int boxesLeft = -1;
    QString text = tr("на склад");
    if (const auto* truckId = std::get_if<TruckId>(&*next)) {
        const Truck* truck = workDay_->findTruck(*truckId);
        boxesLeft = truck->boxesLeft(journal_.loadOf(*truckId));
        text = tr("у фуру %1 · вільно %2 ящ.").arg(QString::fromStdString(truck->plate())).arg(std::max(boxesLeft, 0));
    }
    return {{"text", text}, {"follows", next != target}, {"hangs", false}, {"boxesLeft", boxesLeft}};
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
    const Settlement settlement = wholesale ? Settlement::HandedOver : Settlement::OnSite;
    Receipt receipt = workDay_->closeReceipt(*activeReceiptId_, settlement);
    {
        // Into the journal and out of the open list together: never both or neither after a crash.
        sqlite::ScopedTransaction transaction{db_};
        journalStore_.save(receipt);
        saveSession();
        transaction.commit();
    }
    journal_.add(std::move(receipt));
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
    record(shipment);
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
    return hryvniasText(journal_.balanceOf(*cashSupplierId_) - *paid);
}

QString Terminal::pay(const QString& amount, bool bankTransfer) {
    if (!currentOperatorId_) return tr("Увійдіть як оператор");
    if (!cashSupplierId_) return tr("Оберіть постачальника");
    const auto paid = parseMoney(amount.toStdString());
    if (!paid) return tr("Сума — число гривень, напр. 1500 або 1500,50");
    const Payment payment = Payment::create(*cashSupplierId_, *paid,
                                            bankTransfer ? PaymentMethod::BankTransfer : PaymentMethod::Cash,
                                            *currentOperatorId_);
    if (const auto errors = payment.validate(); !errors.empty()) return joined(errors);
    record(payment);
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
        {"capacity", describeBoxLimit(truck)},
        {"load", describeCargo(load)},
        // Bare numbers for the session table; kilograms are for reference, with no limit.
        {"loadBoxes", QString::number(load.boxes)},
        {"loadKg", kilogramsText(load.weight)},
        {"limitBoxes", QString::number(truck.boxLimit())},
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

QString Terminal::cropName(std::string_view code) const {
    const auto found = std::ranges::find(crops_, code, &Crop::code);
    return QString::fromStdString(found != crops_.end() ? found->name() : std::string{code});
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

std::optional<Destination> Terminal::receivingDestination() const {
    if (receivingTargetId_.isEmpty()) return std::nullopt;
    const auto destination = destinationFor(receivingTargetId_);
    if (const auto* place = std::get_if<Destination>(&destination)) return *place;
    return std::nullopt;
}

QString Terminal::placeName(const Destination& destination) const {
    const auto* truckId = std::get_if<TruckId>(&destination);
    return truckId ? plateOf(*truckId) : tr("склад");
}

QString Terminal::allocate(const SourceReceipt& source, const Allocation& allocation) {
    const auto errors = std::visit([&](const auto* receipt) { return check(journal_, allocation, *receipt); }, source);
    if (!errors.empty()) return joined(errors);
    record(allocation);
    emit journalChanged();
    return {};
}

template <class Record>
void Terminal::record(const Record& r) {
    journalStore_.save(r);
    journal_.add(r);
}

void Terminal::saveSession() {
    if (workDay_) sessionStore_.save(*workDay_);
}

void Terminal::seedDemoData() {
    sqlite::ScopedTransaction transaction{db_};
    const WorkDay yesterday = demoYesterday(operators_.front().id());
    sessionStore_.save(yesterday);
    addDemoHistory(journalStore_, yesterday.trucks()[1].id(), yesterday.openedBy());
    transaction.commit();
}

void Terminal::backup() {
    if (!backupFolder_) return;
    try {
        (void)backupDatabase(db_, *backupFolder_, Clock::now(), kBackupsKept);
    } catch (const std::exception& error) {
        // Receiving berries matters more than the copy; the next start or
        // closed session tries again.
        qWarning("Backup failed: %s", error.what());
    }
}

// Emits always: even for the same id its lines may have changed.
void Terminal::setActiveReceipt(std::optional<Uuid> id) {
    activeReceiptId_ = std::move(id);
    emit receiptsChanged();
    emit currentSupplierChanged();
    emit balanceChanged();
}

} // namespace wms::ui
