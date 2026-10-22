#pragma once

#include "wms/journal.h"
#include "wms/supplier.h"

#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

namespace wms::ui {

// Suppliers matching the operator's search, for a ListView. Reads the data
// owned by Terminal; keeps only the indices of the rows that match.
class SupplierListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Terminal")

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, PhoneRole, BalanceRole };

    SupplierListModel(const std::vector<Supplier>& suppliers, const Journal& journal, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Supplier id of a visible row; 0 for a row outside the list.
    [[nodiscard]] Q_INVOKABLE qint64 idAt(int row) const;

    [[nodiscard]] const QString& query() const noexcept { return query_; }
    void setQuery(const QString& query);

    // Balances are derived from the journal, so rows must be redrawn after it changes.
    void refreshBalances();
    // The supplier list itself changed (one was added).
    void refresh() { applyFilter(); }

signals:
    void queryChanged();
    void countChanged();

private:
    void applyFilter();

    const std::vector<Supplier>& suppliers_;
    const Journal& journal_;
    QString query_;
    std::vector<std::size_t> visible_; // indices into suppliers_
};

} // namespace wms::ui
