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
    // {cropCode: formatted price}; a crop without a price is missing.
    Q_PROPERTY(QVariantMap prices READ priceMap NOTIFY pricesChanged)
    // [{truckId, plate, make, note, capacity, load, fillPercent, overloaded, status, loading}] of the session.
    Q_PROPERTY(QVariantList trucks READ truckList NOTIFY trucksChanged)
    // [{index, plate, make, note, capacity}] of the previous session, for "repeat the truck from yesterday".
    Q_PROPERTY(QVariantList previousTrucks READ previousTruckList NOTIFY sessionChanged)
    // Why the session cannot be closed yet, straight from WorkDay::validateClose().
    Q_PROPERTY(QStringList closeBlockers READ closeBlockers NOTIFY trucksChanged)

    // [{receiptId, supplierName, openedAt, total}] of the session, oldest first.
    Q_PROPERTY(QVariantList openReceipts READ openReceiptList NOTIFY receiptsChanged)
    // The receipt new weighings go into; empty when none is open.
    Q_PROPERTY(QString activeReceiptId READ activeReceiptId NOTIFY receiptsChanged)
    // [{index, crop, boxes, gross, tare, net, price, amount, manualPrice, manualWeight, cancelled}].
    Q_PROPERTY(QVariantList activeLines READ activeLines NOTIFY receiptsChanged)
    // "340 ящ. · 120.000 кг · 10200.00 грн" without cancelled lines.
    Q_PROPERTY(QString activeTotal READ activeTotal NOTIFY receiptsChanged)

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
    [[nodiscard]] QString activeTotal() const;

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

private:
    [[nodiscard]] const Supplier* currentSupplier() const noexcept;
    [[nodiscard]] const Operator* findOperator(OperatorId id) const noexcept;
    [[nodiscard]] QString operatorNameOf(OperatorId id) const;
    [[nodiscard]] QVariantMap truckInfo(const Truck& truck) const;
    [[nodiscard]] const OpenReceipt* activeReceipt() const noexcept;
    void setActiveReceipt(std::optional<Uuid> id);

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
    std::optional<OperatorId> currentOperatorId_;
};

} // namespace wms::ui
