#pragma once

#include "scale_monitor.h"
#include "supplier_list_model.h"
#include "wms/crop.h"
#include "wms/crop_keys.h"
#include "wms/journal.h"
#include "wms/operator.h"
#include "wms/scale.h"
#include "wms/sqlite.h"
#include "wms/sqlite_journal_store.h"
#include "wms/sqlite_repository.h"
#include "wms/sqlite_session_store.h"
#include "wms/supplier.h"
#include "wms/truck.h"
#include "wms/workday.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace wms::ui {

// The only door from QML into the core. Holds the terminal state in memory
// and writes every change to the local database at once, so a restart (or a
// crash) loses nothing; screens call it and show what it returns, all rules
// stay in core.
class Terminal : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int journalSize READ journalSize NOTIFY journalChanged)
    // Fully qualified: the QML type registrar does not resolve names inside the namespace.
    Q_PROPERTY(wms::ui::SupplierListModel* suppliers READ supplierModel CONSTANT)
    Q_PROPERTY(wms::ui::ScaleMonitor* scales READ scaleMonitor CONSTANT)
    // The supplier of the active receipt: whose berries are on the scale.
    Q_PROPERTY(bool hasSupplier READ hasSupplier NOTIFY currentSupplierChanged)
    Q_PROPERTY(QString supplierName READ supplierName NOTIFY currentSupplierChanged)
    Q_PROPERTY(QString supplierBalance READ supplierBalance NOTIFY balanceChanged)
    // [{operatorId, name}] for the login picker; the PIN never leaves the core.
    Q_PROPERTY(QVariantList operators READ operatorList CONSTANT)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY operatorChanged)
    Q_PROPERTY(QString operatorName READ operatorName NOTIFY operatorChanged)

    Q_PROPERTY(bool sessionOpen READ sessionOpen NOTIFY sessionChanged)
    // "Сесія 26.09.2026", empty without an open session.
    Q_PROPERTY(QString sessionTitle READ sessionTitle NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionOpenedBy READ sessionOpenedBy NOTIFY sessionChanged)
    // [{cropCode, name}] in the order added. Kept apart from the prices: a Repeater
    // over a list that changes rebuilds its rows and would wipe what the operator
    // is typing. The list itself changes only when a crop is added.
    Q_PROPERTY(QVariantList crops READ cropList NOTIFY cropsChanged)
    // Codes of the crops that are keys on the receiving screen, in screen order
    // (CropKeys); the rest go to the "Ще" menu.
    Q_PROPERTY(QStringList cropKeys READ cropKeyList NOTIFY cropKeysChanged)
    // {cropCode: price in hryvnias, "85,00"}; a crop without a price is missing.
    Q_PROPERTY(QVariantMap prices READ priceMap NOTIFY pricesChanged)
    // [{truckId, plate, make, note, capacity, load, loadBoxes, loadKg, limitBoxes,
    //   fillPercent, overloaded, status, loading}] of the session.
    Q_PROPERTY(QVariantList trucks READ truckList NOTIFY trucksChanged)
    // [{index, plate, make, note, capacity}] of the previous session, for "repeat the truck from yesterday".
    Q_PROPERTY(QVariantList previousTrucks READ previousTruckList NOTIFY sessionChanged)
    // The day so far, from SessionSummary: {receivedKg, receivedBoxes, crops ("малина 1 920,4 · …"),
    // closedReceipts, openReceipts, inTrucks, inWarehouse, waiting (boxes), paid, paidCash, paidTransfer};
    // weights and money as bare numbers. Empty without an open session.
    Q_PROPERTY(QVariantMap sessionSummary READ sessionSummary NOTIFY summaryChanged)
    // Why the session cannot be closed yet, straight from WorkDay::validateClose().
    // Open receipts block closing as well as loading trucks, so both changes notify.
    Q_PROPERTY(QStringList closeBlockers READ closeBlockers NOTIFY closeBlockersChanged)

    // [{receiptId, supplierName, openedAt, boxes, pallets, amount}] of the session, oldest first; amount as "8 823,00",
    // pallets as "2 палети" (counted lines only).
    Q_PROPERTY(QVariantList openReceipts READ openReceiptList NOTIFY receiptsChanged)
    // The receipt new weighings go into; empty when none is open.
    Q_PROPERTY(QString activeReceiptId READ activeReceiptId NOTIFY receiptsChanged)
    // [{index, crop, boxes, gross, tare, net, price, amount, manualPrice, manualWeight, cancelled, placedIn, hanging}];
    // weights in kg and money in hryvnias as bare numbers ("142,6", "8 823,00") for a table; placedIn is
    // a plate or "склад", hanging = a pallet left for the allocation screen.
    Q_PROPERTY(QVariantList activeLines READ activeLines NOTIFY receiptsChanged)
    // {openedAt, boxes, net, amount} of the active receipt without cancelled lines; empty when none.
    Q_PROPERTY(QVariantMap activeSummary READ activeSummary NOTIFY receiptsChanged)
    // The box tare a new receiving form starts with, in kilograms as typed ("1,2");
    // set per terminal from the app bar menu, since crates differ between points.
    Q_PROPERTY(QString defaultBoxTare READ defaultBoxTare NOTIFY defaultBoxTareChanged)
    // Where weighed pallets go unless an earlier pallet of the crop in the receipt leads
    // (destinationId as in `destinations`); empty: none, they hang. Remembered per terminal.
    Q_PROPERTY(QString receivingTarget READ receivingTarget NOTIFY receivingTargetChanged)
    // Where the next pallet of each crop goes, said before it is weighed: {cropCode: {text, plate, follows,
    // hangs, boxesLeft}}. text: "у фуру BI1111AA" / "на склад"; plate is empty for the warehouse;
    // follows: after the last pallet of the crop rather than the target; boxesLeft is -1 for the
    // warehouse. Empty without an active receipt.
    Q_PROPERTY(QVariantMap nextPlacements READ nextPlacements NOTIFY placementsChanged)

    // Allocation. [{receiptId, supplierName, openedAt, open, leftBoxes, leftKg}]: open receipts
    // of the session, then closed ones, that still have boxes to allocate.
    Q_PROPERTY(QVariantList allocationSources READ allocationSources NOTIFY allocationChanged)
    // The receipt the allocation screen works on; independent of the active one,
    // so loading a truck never changes where the next weighing goes.
    Q_PROPERTY(QString allocationReceiptId READ allocationReceiptId NOTIFY allocationChanged)
    Q_PROPERTY(QString allocationSupplierName READ allocationSupplierName NOTIFY allocationChanged)
    // [{index, crop, boxes, net, cancelled, placedIn}], net in kg as a bare number; placedIn is empty while the pallet is free.
    Q_PROPERTY(QVariantList allocationLines READ allocationLines NOTIFY allocationChanged)
    // [{cropCode, name, leftBoxes, leftKg}] of the selected receipt, only crops with boxes left.
    Q_PROPERTY(QVariantList allocationCrops READ allocationCrops NOTIFY allocationChanged)
    // [{destinationId, title, detail, loadBoxes, loadKg, limitBoxes, fillPercent, overloaded,
    //   warehouse}]: trucks of the session still loading, then the warehouse (destinationId "warehouse").
    Q_PROPERTY(QVariantList destinations READ destinationList NOTIFY allocationChanged)

    // Warehouse. [{cropCode, name, boxes, kg, empty, negative}] for every crop (kg a bare number), across all
    // sessions; negative after an offline conflict, shown so the logistician can sort it out.
    Q_PROPERTY(QVariantList stock READ stockList NOTIFY journalChanged)

    // Cash desk. [{supplierId, name, balance, overpaid}] of suppliers whose balance is
    // not zero, biggest debt first; overpaid ones (they owe us) at the end.
    Q_PROPERTY(QVariantList debtors READ debtorList NOTIFY journalChanged)
    // The supplier the cash screen works on; 0 when none is chosen. Independent
    // of the active receipt, so paying one supplier never touches the scale.
    Q_PROPERTY(qint64 cashSupplierId READ cashSupplierId NOTIFY cashChanged)
    Q_PROPERTY(QString cashSupplierName READ cashSupplierName NOTIFY cashChanged)
    Q_PROPERTY(QString cashBalance READ cashBalance NOTIFY cashChanged)
    // The whole debt as the operator would type it ("1234,50"); empty when we owe nothing.
    Q_PROPERTY(QString cashDebtInput READ cashDebtInput NOTIFY cashChanged)
    // [{at, title, detail, effect, payment}] of the chosen supplier, oldest first.
    // Sums here and in debtors/cashBalance/balanceAfter are bare hryvnias ("8 823,00", "−50,00").
    Q_PROPERTY(QVariantList cashHistory READ cashHistory NOTIFY cashChanged)

public:
    explicit Terminal(QObject* parent = nullptr);

    [[nodiscard]] int journalSize() const noexcept;
    [[nodiscard]] SupplierListModel* supplierModel() noexcept { return &supplierModel_; }
    [[nodiscard]] ScaleMonitor* scaleMonitor() noexcept { return &scaleMonitor_; }
    [[nodiscard]] bool hasSupplier() const noexcept { return currentSupplier() != nullptr; }
    [[nodiscard]] QString supplierName() const;
    // What we owe the current supplier, formatted; empty without a supplier.
    [[nodiscard]] QString supplierBalance() const;

    [[nodiscard]] QVariantList operatorList() const;
    [[nodiscard]] bool loggedIn() const noexcept { return currentOperatorId_.has_value(); }
    [[nodiscard]] QString operatorName() const;

    [[nodiscard]] bool sessionOpen() const noexcept { return workDay_.has_value(); }
    [[nodiscard]] QString sessionTitle() const;
    [[nodiscard]] QString sessionOpenedBy() const;
    [[nodiscard]] QVariantList cropList() const;
    [[nodiscard]] QStringList cropKeyList() const;
    [[nodiscard]] QVariantMap priceMap() const;
    [[nodiscard]] QVariantList truckList() const;
    [[nodiscard]] QVariantList previousTruckList() const;
    [[nodiscard]] QVariantMap sessionSummary() const;
    [[nodiscard]] QStringList closeBlockers() const;
    [[nodiscard]] QVariantList openReceiptList() const;
    [[nodiscard]] QString activeReceiptId() const;
    [[nodiscard]] QVariantList activeLines() const;
    [[nodiscard]] QVariantMap activeSummary() const;
    [[nodiscard]] QString receivingTarget() const;
    [[nodiscard]] QString defaultBoxTare() const { return defaultBoxTare_; }
    [[nodiscard]] QVariantMap nextPlacements() const;
    [[nodiscard]] QVariantList allocationSources() const;
    [[nodiscard]] QString allocationReceiptId() const;
    [[nodiscard]] QString allocationSupplierName() const;
    [[nodiscard]] QVariantList allocationLines() const;
    [[nodiscard]] QVariantList allocationCrops() const;
    [[nodiscard]] QVariantList destinationList() const;
    [[nodiscard]] QVariantList stockList() const;
    [[nodiscard]] QVariantList debtorList() const;
    [[nodiscard]] qint64 cashSupplierId() const noexcept { return cashSupplierId_.value_or(0); }
    [[nodiscard]] QString cashSupplierName() const;
    [[nodiscard]] QString cashBalance() const;
    [[nodiscard]] QString cashDebtInput() const;
    [[nodiscard]] QVariantList cashHistory() const;

    // Returns false for an unknown operator or a wrong PIN.
    Q_INVOKABLE bool login(qint64 operatorId, const QString& pin);
    // Open receipts stay with the session; the next operator just picks one again.
    Q_INVOKABLE void logout();

    // Opened by the logged-in operator, named by today's date. Trucks carried
    // over from the previous session join it with their load. Does nothing
    // without an operator or when a session is already open.
    Q_INVOKABLE void openSession();
    // Session editing: each returns an operator-facing error, empty on success.
    Q_INVOKABLE QString setPrice(const QString& cropCode, const QString& price);
    // A crop that came into season: why it cannot be added (shown while
    // typing), and adding it. Returns an operator-facing error, empty on success.
    [[nodiscard]] Q_INVOKABLE QStringList newCropErrors(const QString& name) const;
    Q_INVOKABLE QString addCrop(const QString& name);
    // Removing a crop: deleted when nothing names it (a typo), archived when
    // records do. cropRemoval says beforehand: {errors, keepsRecords}.
    [[nodiscard]] Q_INVOKABLE QVariantMap cropRemoval(const QString& cropCode) const;
    Q_INVOKABLE QString removeCrop(const QString& cropCode);
    // Picked from the menu, weighed or priced: a crop from the menu takes the
    // place of the key used longest ago. Remembered per terminal.
    Q_INVOKABLE void useCrop(const QString& cropCode);
    // The box limit as typed; make and note are optional.
    Q_INVOKABLE QString addTruck(const QString& plate, const QString& make, const QString& note,
                                 const QString& boxes);
    Q_INVOKABLE QString repeatTruck(int previousIndex);
    Q_INVOKABLE void departTruck(const QString& truckId);
    Q_INVOKABLE void carryOverTruck(const QString& truckId);
    // Does nothing while closeBlockers is not empty.
    Q_INVOKABLE void closeSession();

    // Receiving. The supplier's open receipt, or a new one, becomes active;
    // returns an operator-facing error, empty on success.
    Q_INVOKABLE QString startReceipt(qint64 supplierId);
    // A new supplier typed at the scale: why it cannot be added (shown while
    // typing), and adding it. addSupplier returns the new id, 0 when refused.
    [[nodiscard]] Q_INVOKABLE QStringList newSupplierErrors(const QString& fullName, const QString& phone) const;
    Q_INVOKABLE qint64 addSupplier(const QString& fullName, const QString& phone);
    Q_INVOKABLE void activateReceipt(const QString& receiptId);
    // A weighing into the active receipt, at the day price; the operator
    // changes it only when paying. Weights in kilograms as typed. A typed
    // weight is recorded as manual; otherwise the scale must be stable, unless
    // the operator accepts the unstable reading.
    Q_INVOKABLE QString recordWeighing(const QString& cropCode, const QString& boxes, const QString& tarePerBox,
                                       const QString& palletTare, const QString& manualWeight, bool acceptUnstable);
    // Returns why the line cannot be cancelled, empty when it was.
    Q_INVOKABLE QString cancelWeighing(int index);
    Q_INVOKABLE void setReceivingTarget(const QString& destinationId);
    // Returns why the value is not a box tare; empty when it was saved.
    Q_INVOKABLE QString setDefaultBoxTare(const QString& kilograms);
    // Closing the active receipt; afterwards the next open one becomes active.
    // "Pay" (retail): [{cropCode, crop, boxes, net, price}], one row per crop,
    // the price prefilled with the current day price as the operator types it.
    [[nodiscard]] Q_INVOKABLE QVariantList payRows() const;
    // For typed prices {cropCode: text}: {amounts: {cropCode: "8 823,00"}, total, blockers}.
    [[nodiscard]] Q_INVOKABLE QVariantMap payCheck(const QVariantMap& prices) const;
    // Closes as retail and pays the whole amount at once. Returns an
    // operator-facing error, empty on success.
    Q_INVOKABLE QString payReceipt(const QVariantMap& prices, bool bankTransfer);
    // "Issue receipt" (wholesale): weight only, priced outside the terminal.
    [[nodiscard]] Q_INVOKABLE QStringList handOverBlockers() const;
    Q_INVOKABLE void handOverReceipt();

    // Allocation of the selected receipt into a truck or the warehouse
    // (destinationId as in `destinations`). Each returns an operator-facing
    // error, empty on success.
    Q_INVOKABLE void selectAllocationReceipt(const QString& receiptId);
    Q_INVOKABLE QString allocateWeighing(int index, const QString& destinationId);
    Q_INVOKABLE QString allocateBoxes(const QString& cropCode, const QString& boxes, const QString& destinationId);

    // Boxes of a crop from the warehouse into a truck of the session; the weight
    // is split from the stock. Returns an operator-facing error, empty on success.
    Q_INVOKABLE QString shipFromStock(const QString& cropCode, const QString& boxes, const QString& truckId);

    // Cash desk. A payment to the chosen supplier; the amount in hryvnias as typed.
    // Paying more than the debt is allowed: the supplier then owes us.
    Q_INVOKABLE void selectCashSupplier(qint64 supplierId);
    // What the chosen supplier's balance becomes after paying `amount`; empty for bad input.
    [[nodiscard]] Q_INVOKABLE QString balanceAfter(const QString& amount) const;
    // Returns an operator-facing error, empty on success.
    Q_INVOKABLE QString pay(const QString& amount, bool bankTransfer);

    // Scale simulator controls, until real scales are connected.
    Q_INVOKABLE void simulatePallet();
    Q_INVOKABLE void simulateEmptyScale();

    // Read access for the C++ list models; QML goes through properties.
    [[nodiscard]] const std::vector<Supplier>& suppliers() const noexcept { return suppliers_; }
    [[nodiscard]] const Journal& journal() const noexcept { return journal_; }

signals:
    void operatorChanged();
    void journalChanged();
    void currentSupplierChanged();
    // The balance depends on both the supplier and the journal.
    void balanceChanged();
    // Opened or closed.
    void sessionChanged();
    void cropsChanged();
    void cropKeysChanged();
    void pricesChanged();
    // Truck loads depend on both the session and the journal.
    void trucksChanged();
    // Open receipts, their lines or which one is active.
    void receiptsChanged();
    // Anything the allocation screen shows: receipts, trucks, the journal.
    void allocationChanged();
    void summaryChanged();
    void closeBlockersChanged();
    // The chosen supplier of the cash screen or its records.
    void cashChanged();
    // Chosen, or its truck stopped loading.
    void receivingTargetChanged();
    // The active receipt, the trucks or the target changed.
    void placementsChanged();
    void defaultBoxTareChanged();

private:
    [[nodiscard]] const Supplier* currentSupplier() const noexcept;
    [[nodiscard]] const Operator* findOperator(OperatorId id) const noexcept;
    [[nodiscard]] QString operatorNameOf(OperatorId id) const;
    [[nodiscard]] QVariantMap truckInfo(const Truck& truck) const;
    [[nodiscard]] const OpenReceipt* activeReceipt() const noexcept;
    void setActiveReceipt(std::optional<Uuid> id);
    // A receipt to allocate from is either still open in the session or closed in the journal.
    using SourceReceipt = std::variant<const OpenReceipt*, const Receipt*>;
    [[nodiscard]] std::optional<SourceReceipt> findSource(const Uuid& id) const noexcept;
    [[nodiscard]] QString supplierNameOf(SupplierId id) const;
    // The code itself for a crop missing from the catalogue, e.g. one synced in later.
    // Archived crops keep their names here.
    [[nodiscard]] QString cropName(std::string_view code) const;
    [[nodiscard]] bool isActiveCrop(std::string_view code) const noexcept;
    // Typed prices of the active receipt's crops; a crop whose price does not
    // parse is left out and named in `errors`.
    [[nodiscard]] std::vector<CropPrice> typedPrices(const QVariantMap& prices, QStringList& errors) const;
    // The receipt is closed: stored and journalled with what came with it, in one transaction.
    void receiptClosed(Receipt receipt, std::optional<Payment> payment);
    // In the journal or in a receipt still being weighed.
    [[nodiscard]] bool hasCropRecords(std::string_view code) const noexcept;
    void saveCropKeys() const;
    // After a crop is added, brought back or removed.
    void cropsUpdated();
    // Plate of a truck of this or the previous session.
    [[nodiscard]] QString plateOf(const TruckId& id) const;
    // Checks the operator's choice: a truck must be of this session and still loading.
    [[nodiscard]] std::variant<Destination, QString> destinationFor(const QString& destinationId) const;
    // The receiving target when it still takes berries.
    [[nodiscard]] std::optional<Destination> receivingDestination() const;
    // A plate or "склад", for a table cell.
    [[nodiscard]] QString placeName(const Destination& destination) const;
    [[nodiscard]] QVariantMap nextPlacement(std::string_view cropCode) const;
    // Validates against the journal and adds; returns the errors, joined.
    QString allocate(const SourceReceipt& source, const Allocation& allocation);
    // A journal record: stored first, then added, so the screen never shows
    // what a restart would lose.
    template <class Record>
    void record(const Record& r);
    // After every change of the open session.
    void saveSession();
    // A new database gets yesterday's session and some history, so every
    // screen has something to show in the alpha.
    void seedDemoData();
    // A copy of the database into backupFolder_; a failure is only logged.
    void backup();

    const std::string databasePath_;
    // Empty for an in-memory database: nothing to keep.
    const std::optional<std::filesystem::path> backupFolder_;
    // First of the data: everything below is loaded from it.
    sqlite::Database db_;
    SqliteJournalStore journalStore_;
    SqliteSessionStore sessionStore_;
    SqliteRepository<Operator> operatorStore_;
    SqliteRepository<Supplier> supplierStore_;
    SqliteRepository<Crop> cropStore_;
    std::vector<Operator> operators_;
    std::vector<Supplier> suppliers_;
    std::vector<Crop> crops_;
    CropKeys cropKeys_;
    // Of the session closed last: repeat and carry-over read them.
    std::vector<Truck> previousTrucks_;
    std::optional<WorkDay> workDay_;
    Journal journal_;
    // Declared after the data it reads, so it is constructed after it.
    SupplierListModel supplierModel_;
    SimulatedScaleDriver simulator_;
    ScaleMonitor scaleMonitor_;
    // An id, not a pointer: the open receipts live in a vector that reallocates.
    std::optional<Uuid> activeReceiptId_;
    std::optional<Uuid> allocationReceiptId_;
    std::optional<SupplierId> cashSupplierId_;
    std::optional<OperatorId> currentOperatorId_;
    QString receivingTargetId_;
    QString defaultBoxTare_;
};

} // namespace wms::ui
