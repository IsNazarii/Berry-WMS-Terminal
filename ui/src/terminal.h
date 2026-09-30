#pragma once

#include "scale_monitor.h"
#include "supplier_list_model.h"
#include "wms/journal.h"
#include "wms/operator.h"
#include "wms/scale.h"
#include "wms/supplier.h"
#include "wms/truck.h"
#include "wms/workday.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <optional>
#include <variant>
#include <vector>

namespace wms::ui {

// The only door from QML into the core. Holds the terminal state in memory
// until the SQLite repositories exist; screens call it and show what it
// returns, all rules stay in core.
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
    // [{cropCode, name}]. Kept apart from the prices: a Repeater over a list that
    // changes rebuilds its rows and would wipe what the operator is typing.
    Q_PROPERTY(QVariantList crops READ cropList CONSTANT)
    // {cropCode: price in hryvnias, "85,00"}; a crop without a price is missing.
    Q_PROPERTY(QVariantMap prices READ priceMap NOTIFY pricesChanged)
    // [{truckId, plate, make, note, capacity, load, loadBoxes, loadKg, limitBoxes, limitKg,
    //   fillPercent, overloaded, status, loading}] of the session.
    Q_PROPERTY(QVariantList trucks READ truckList NOTIFY trucksChanged)
    // [{index, plate, make, note, capacity}] of the previous session, for "repeat the truck from yesterday".
    Q_PROPERTY(QVariantList previousTrucks READ previousTruckList NOTIFY sessionChanged)
    // Why the session cannot be closed yet, straight from WorkDay::validateClose().
    Q_PROPERTY(QStringList closeBlockers READ closeBlockers NOTIFY trucksChanged)

    // [{receiptId, supplierName, openedAt, boxes, amount}] of the session, oldest first; amount as "8 823,00".
    Q_PROPERTY(QVariantList openReceipts READ openReceiptList NOTIFY receiptsChanged)
    // The receipt new weighings go into; empty when none is open.
    Q_PROPERTY(QString activeReceiptId READ activeReceiptId NOTIFY receiptsChanged)
    // [{index, crop, boxes, gross, tare, net, price, amount, manualPrice, manualWeight, cancelled}];
    // weights in kg and money in hryvnias as bare numbers ("142,6", "8 823,00") for a table.
    Q_PROPERTY(QVariantList activeLines READ activeLines NOTIFY receiptsChanged)
    // {openedAt, boxes, net, amount} of the active receipt without cancelled lines; empty when none.
    Q_PROPERTY(QVariantMap activeSummary READ activeSummary NOTIFY receiptsChanged)

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
    // [{destinationId, title, detail, loadBoxes, loadKg, limitBoxes, limitKg, fillPercent, overloaded,
    //   warehouse}]: trucks of the session still loading, then the warehouse (destinationId "warehouse").
    Q_PROPERTY(QVariantList destinations READ destinationList NOTIFY allocationChanged)

    // Warehouse. [{cropCode, name, boxes, stock, negative}] for every crop, across all
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
    [[nodiscard]] QVariantMap priceMap() const;
    [[nodiscard]] QVariantList truckList() const;
    [[nodiscard]] QVariantList previousTruckList() const;
    [[nodiscard]] QStringList closeBlockers() const;
    [[nodiscard]] QVariantList openReceiptList() const;
    [[nodiscard]] QString activeReceiptId() const;
    [[nodiscard]] QVariantList activeLines() const;
    [[nodiscard]] QVariantMap activeSummary() const;
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
    // Limits as typed; an empty field means no limit in that dimension.
    // Make and note are optional.
    Q_INVOKABLE QString addTruck(const QString& plate, const QString& make, const QString& note,
                                 const QString& boxes, const QString& kilograms);
    Q_INVOKABLE QString repeatTruck(int previousIndex);
    Q_INVOKABLE void departTruck(const QString& truckId);
    Q_INVOKABLE void carryOverTruck(const QString& truckId);
    // Does nothing while closeBlockers is not empty.
    Q_INVOKABLE void closeSession();

    // Receiving. The supplier's open receipt, or a new one, becomes active;
    // returns an operator-facing error, empty on success.
    Q_INVOKABLE QString startReceipt(qint64 supplierId);
    Q_INVOKABLE void activateReceipt(const QString& receiptId);
    // A weighing into the active receipt. Weights in kilograms as typed. An
    // empty price takes the day price, a typed one is a manual price. A typed
    // weight is recorded as manual; otherwise the scale must be stable, unless
    // the operator accepts the unstable reading.
    Q_INVOKABLE QString recordWeighing(const QString& cropCode, const QString& boxes, const QString& tarePerBox,
                                       const QString& palletTare, const QString& price, const QString& manualWeight,
                                       bool acceptUnstable);
    // Returns why the line cannot be cancelled, empty when it was.
    Q_INVOKABLE QString cancelWeighing(int index);
    // Why the active receipt cannot be closed as retail / wholesale yet.
    [[nodiscard]] Q_INVOKABLE QStringList receiptCloseBlockers(bool wholesale) const;
    // The receipt goes into the journal and the next open one becomes active.
    Q_INVOKABLE void closeReceipt(bool wholesale);

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
    void pricesChanged();
    // Truck loads depend on both the session and the journal.
    void trucksChanged();
    // Open receipts, their lines or which one is active.
    void receiptsChanged();
    // Anything the allocation screen shows: receipts, trucks, the journal.
    void allocationChanged();
    // The chosen supplier of the cash screen or its records.
    void cashChanged();

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
    // Plate of a truck of this or the previous session.
    [[nodiscard]] QString plateOf(const TruckId& id) const;
    // Checks the operator's choice: a truck must be of this session and still loading.
    [[nodiscard]] std::variant<Destination, QString> destinationFor(const QString& destinationId) const;
    // Validates against the journal and adds; returns the errors, joined.
    QString allocate(const SourceReceipt& source, const Allocation& allocation);

    std::vector<Operator> operators_;
    std::vector<Supplier> suppliers_;
    // Kept while there is no open session: repeat and carry-over read them.
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
};

} // namespace wms::ui
