#pragma once

#include "supplier_list_model.h"
#include "wms/journal.h"
#include "wms/supplier.h"
#include "wms/truck.h"

#include <QObject>
#include <QString>
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
    Q_PROPERTY(bool hasSupplier READ hasSupplier NOTIFY currentSupplierChanged)
    Q_PROPERTY(qint64 supplierId READ supplierId NOTIFY currentSupplierChanged)
    Q_PROPERTY(QString supplierName READ supplierName NOTIFY currentSupplierChanged)
    Q_PROPERTY(QString supplierBalance READ supplierBalance NOTIFY balanceChanged)

public:
    explicit Terminal(QObject* parent = nullptr);

    [[nodiscard]] int journalSize() const noexcept;
    [[nodiscard]] SupplierListModel* supplierModel() noexcept { return &supplierModel_; }
    [[nodiscard]] bool hasSupplier() const noexcept { return currentSupplierId_.has_value(); }
    // Meaningful only when hasSupplier is true.
    [[nodiscard]] qint64 supplierId() const noexcept { return currentSupplierId_.value_or(0); }
    [[nodiscard]] QString supplierName() const;
    // What we owe the current supplier, formatted; empty without a supplier.
    [[nodiscard]] QString supplierBalance() const;

    // Returns false when there is no supplier with this id.
    Q_INVOKABLE bool selectSupplier(qint64 id);
    Q_INVOKABLE void clearSupplier();

    // Read access for the C++ list models; QML goes through properties.
    [[nodiscard]] const std::vector<Supplier>& suppliers() const noexcept { return suppliers_; }
    [[nodiscard]] const std::vector<Truck>& trucks() const noexcept { return trucks_; }
    [[nodiscard]] const Journal& journal() const noexcept { return journal_; }

signals:
    void journalChanged();
    void currentSupplierChanged();
    // The balance depends on both the supplier and the journal.
    void balanceChanged();

private:
    [[nodiscard]] const Supplier* currentSupplier() const noexcept;

    std::vector<Supplier> suppliers_;
    std::vector<Truck> trucks_;
    Journal journal_;
    // Declared after the data it reads, so it is constructed after it.
    SupplierListModel supplierModel_;
    std::optional<SupplierId> currentSupplierId_;
};

} // namespace wms::ui
