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
    Q_PROPERTY(bool hasSupplier READ hasSupplier NOTIFY currentSupplierChanged)
    Q_PROPERTY(qint64 supplierId READ supplierId NOTIFY currentSupplierChanged)
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

public:
    explicit Terminal(QObject* parent = nullptr);

    [[nodiscard]] int journalSize() const noexcept;
    [[nodiscard]] SupplierListModel* supplierModel() noexcept { return &supplierModel_; }
    [[nodiscard]] ScaleMonitor* scaleMonitor() noexcept { return &scaleMonitor_; }
    [[nodiscard]] bool hasSupplier() const noexcept { return currentSupplierId_.has_value(); }
    // Meaningful only when hasSupplier is true.
    [[nodiscard]] qint64 supplierId() const noexcept { return currentSupplierId_.value_or(0); }
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

    // Returns false for an unknown operator or a wrong PIN.
    Q_INVOKABLE bool login(qint64 operatorId, const QString& pin);
    // Also forgets the supplier: the next operator starts from a clean screen.
    Q_INVOKABLE void logout();

    // Returns false when there is no supplier with this id.
    Q_INVOKABLE bool selectSupplier(qint64 id);
    Q_INVOKABLE void clearSupplier();

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

private:
    [[nodiscard]] const Supplier* currentSupplier() const noexcept;
    [[nodiscard]] const Operator* findOperator(OperatorId id) const noexcept;
    [[nodiscard]] QString operatorNameOf(OperatorId id) const;
    [[nodiscard]] QVariantMap truckInfo(const Truck& truck) const;

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
    std::optional<SupplierId> currentSupplierId_;
    std::optional<OperatorId> currentOperatorId_;
};

} // namespace wms::ui
