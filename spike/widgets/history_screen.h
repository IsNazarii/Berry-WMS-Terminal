#pragma once

#include <QAbstractTableModel>
#include <QVariantList>
#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QListWidget;
class QStackedWidget;
class QTableView;

namespace wms::ui {
class Terminal;
}

namespace spike {

class EmptyState;

// The rows of Terminal::historyRows as table columns.
class HistoryTableModel : public QAbstractTableModel {
public:
    enum Column { Time, Place, From, Crop, Pallet, Boxes, Kg, By, ColumnCount };

    using QAbstractTableModel::QAbstractTableModel;
    void setRows(QVariantList rows, bool warehouse);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    [[nodiscard]] QString text(const QVariantMap& row, int column) const;

    QVariantList rows_;
    bool warehouse_ = false;
};

// What a work day loaded and where from, as HistoryScreen.qml.
class HistoryScreen : public QWidget {
public:
    explicit HistoryScreen(wms::ui::Terminal& terminal, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    // Everything is read again from the terminal: there are no bindings.
    void refresh();
    void applyWidths();

    wms::ui::Terminal& terminal_;
    HistoryTableModel model_;
    QFrame* sidePanel_;
    QComboBox* dayPicker_;
    QLabel* dayDetail_;
    QListWidget* places_;
    QLabel* title_;
    QLabel* summary_;
    QStackedWidget* body_;
    QTableView* table_;
    EmptyState* empty_;
    bool narrow_ = false;
};

} // namespace spike
