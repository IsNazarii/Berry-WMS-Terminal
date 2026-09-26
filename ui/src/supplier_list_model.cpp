#include "supplier_list_model.h"

namespace wms::ui {

SupplierListModel::SupplierListModel(const std::vector<Supplier>& suppliers, const Journal& journal,
                                     QObject* parent)
    : QAbstractListModel{parent}, suppliers_{suppliers}, journal_{journal} {
    applyFilter();
}

int SupplierListModel::rowCount(const QModelIndex& parent) const {
    // A list model has no children; Qt asks with a valid parent to find that out.
    return parent.isValid() ? 0 : static_cast<int>(visible_.size());
}

QVariant SupplierListModel::data(const QModelIndex& index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) return {};
    const Supplier& supplier = suppliers_[visible_[static_cast<std::size_t>(index.row())]];
    switch (role) {
    case IdRole: return QVariant::fromValue<qint64>(supplier.id());
    case NameRole: return QString::fromStdString(supplier.fullName());
    case PhoneRole: return QString::fromStdString(supplier.phone());
    case BalanceRole: return QString::fromStdString(formatMoney(journal_.balanceOf(supplier.id())));
    default: return {};
    }
}

QHash<int, QByteArray> SupplierListModel::roleNames() const {
    return {
        {IdRole, "supplierId"}, {NameRole, "name"}, {PhoneRole, "phone"}, {BalanceRole, "balance"},
    };
}

qint64 SupplierListModel::idAt(int row) const {
    if (row < 0 || row >= rowCount()) return 0;
    return suppliers_[visible_[static_cast<std::size_t>(row)]].id();
}

void SupplierListModel::setQuery(const QString& query) {
    if (query == query_) return;
    query_ = query;
    applyFilter();
    emit queryChanged();
}

void SupplierListModel::refreshBalances() {
    if (visible_.empty()) return;
    emit dataChanged(index(0), index(rowCount() - 1), {BalanceRole});
}

void SupplierListModel::applyFilter() {
    const int oldCount = rowCount();
    const std::string needle = query_.trimmed().toStdString();
    // A reset is simpler than computing inserted/removed ranges, and the list is small.
    beginResetModel();
    visible_.clear();
    for (std::size_t i = 0; i < suppliers_.size(); ++i) {
        if (needle.empty() || suppliers_[i].matches(needle)) visible_.push_back(i);
    }
    endResetModel();
    if (rowCount() != oldCount) emit countChanged();
}

} // namespace wms::ui
