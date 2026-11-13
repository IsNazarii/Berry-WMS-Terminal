#include "history_screen.h"

#include "delegates.h"
#include "empty_state.h"
#include "theme.h"

#include "../../ui/src/terminal.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace spike {

namespace {

QLabel* label(const char* role, const QString& text = {}) {
    auto* l = new QLabel{text};
    l->setProperty("role", role);
    return l;
}

// QSS cannot uppercase or space letters: the caption does it in code.
QLabel* caption(const QString& text) {
    QLabel* l = label("caption", text.toUpper());
    QFont font = l->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    l->setFont(font);
    return l;
}

QFrame* panel() {
    auto* frame = new QFrame;
    frame->setObjectName("panel");
    return frame;
}

} // namespace

void HistoryTableModel::setRows(QVariantList rows, bool warehouse) {
    beginResetModel();
    rows_ = std::move(rows);
    warehouse_ = warehouse;
    endResetModel();
}

int HistoryTableModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(rows_.size()); }

int HistoryTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : ColumnCount; }

QString HistoryTableModel::text(const QVariantMap& row, int column) const {
    switch (column) {
    case Time: return row["time"].toString();
    case Place:
        // On the warehouse view a row either came in or left for a truck.
        if (!warehouse_) return row["place"].toString();
        return row["out"].toBool() ? "→ " + row["place"].toString() : tr("на склад");
    case From: return row["from"].toString();
    case Crop: return row["crop"].toString();
    case Pallet: return row["pallet"].toString();
    case Boxes: return row["boxes"].toString();
    case Kg: return row["kg"].toString();
    case By: return row["by"].toString();
    }
    return {};
}

QVariant HistoryTableModel::data(const QModelIndex& index, int role) const {
    const QVariantMap row = rows_.at(index.row()).toMap();
    const int column = index.column();
    const bool reversed = row["reversed"].toBool();
    switch (role) {
    case Qt::DisplayRole: return text(row, column);
    case Qt::TextAlignmentRole:
        return int(column == Pallet || column == Boxes || column == Kg ? Qt::AlignRight : Qt::AlignLeft);
    // The pallet number and who loaded stay readable: they identify the mistake.
    case StruckRole: return reversed && column != Pallet && column != By;
    case MutedRole: return column == By || (column == From && row["out"].toBool()) || reversed;
    case BoldRole: return column == From;
    case NoteRole: return column == From && reversed ? row["reversedNote"] : QVariant{};
    }
    return {};
}

QVariant HistoryTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal) return {};
    if (role == Qt::TextAlignmentRole)
        return int((section == Pallet || section == Boxes || section == Kg ? Qt::AlignRight : Qt::AlignLeft) |
                   Qt::AlignVCenter);
    if (role != Qt::DisplayRole) return {};
    static const QStringList titles{tr("Час"),  tr("Фура"), tr("Від кого"),  tr("Культура"),
                                    tr("Пал."), tr("Ящ."),  tr("Нетто, кг"), tr("Хто")};
    if (section == Place && warehouse_) return tr("Куди").toUpper();
    return titles.at(section).toUpper();
}

HistoryScreen::HistoryScreen(wms::ui::Terminal& terminal, QWidget* parent)
    : QWidget{parent}, terminal_{terminal}, sidePanel_{panel()}, dayPicker_{new QComboBox}, dayDetail_{label("muted")},
      places_{new QListWidget}, title_{label("title")}, summary_{label("muted")}, body_{new QStackedWidget},
      table_{new QTableView}, empty_{new EmptyState{"history"}} {
    auto* side = new QVBoxLayout{sidePanel_};
    side->setContentsMargins(kSpacingM, kSpacingM, kSpacingM, kSpacingM);
    side->setSpacing(kSpacingM);
    side->addWidget(caption(tr("День")));
    side->addWidget(dayPicker_);
    side->addWidget(dayDetail_);
    side->addWidget(caption(tr("Куди вантажили")));
    side->addWidget(places_, 1);
    places_->setItemDelegate(new ListRowDelegate{places_});
    places_->setMouseTracking(true);
    places_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    table_->setModel(&model_);
    table_->setItemDelegate(new TableCellDelegate{table_});
    table_->setShowGrid(false);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(48);
    table_->horizontalHeader()->setFixedHeight(28);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(HistoryTableModel::From, QHeaderView::Stretch);
    body_->addWidget(table_);
    body_->addWidget(empty_);

    QFrame* main = panel();
    auto* right = new QVBoxLayout{main};
    right->setContentsMargins(kSpacingM, kSpacingM, kSpacingM, kSpacingM);
    right->setSpacing(2);
    right->addWidget(title_);
    right->addWidget(summary_);
    right->addSpacing(kSpacingM);
    right->addWidget(body_, 1);

    auto* layout = new QHBoxLayout{this};
    layout->setContentsMargins(kSpacingM, kSpacingM, kSpacingM, kSpacingM);
    layout->setSpacing(kSpacingM);
    layout->addWidget(sidePanel_);
    layout->addWidget(main, 1);

    connect(dayPicker_, &QComboBox::activated, this,
            [this](int index) { terminal_.selectHistorySession(dayPicker_->itemData(index).toString()); });
    connect(places_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) terminal_.selectHistoryPlace(places_->item(row)->data(Qt::UserRole).toString());
    });
    connect(&terminal_, &wms::ui::Terminal::historyChanged, this, &HistoryScreen::refresh);
    applyWidths();
    refresh();
}

void HistoryScreen::refresh() {
    const bool hasDay = !terminal_.historySessionId().isEmpty();
    const QString place = terminal_.historyPlace();
    const bool warehouse = place == "warehouse";
    {
        // Filling the controls must not echo back as the operator's choice.
        const QSignalBlocker blockDays{dayPicker_};
        const QSignalBlocker blockPlaces{places_};
        dayPicker_->clear();
        for (const QVariant& s : terminal_.historySessions()) {
            const QVariantMap session = s.toMap();
            dayPicker_->addItem(session["title"].toString(), session["sessionId"]);
            if (session["sessionId"] == terminal_.historySessionId()) {
                dayPicker_->setCurrentIndex(dayPicker_->count() - 1);
                dayDetail_->setText(session["detail"].toString());
            }
        }
        dayPicker_->setEnabled(hasDay);
        dayDetail_->setVisible(hasDay);

        places_->clear();
        for (const QVariant& p : terminal_.historyPlaces()) {
            const QVariantMap info = p.toMap();
            auto* item = new QListWidgetItem{info["title"].toString(), places_};
            item->setData(Qt::UserRole, info["placeId"]);
            item->setData(TitleRole, info["title"]);
            item->setData(DetailRole, info["detail"]);
            item->setData(TrailingRole, info["rows"].toString());
            if (info["placeId"] == place) {
                places_->setCurrentItem(item);
                title_->setText(info["title"].toString());
            }
        }
    }
    title_->setVisible(hasDay);
    summary_->setVisible(hasDay);
    summary_->setText(terminal_.historySummary());

    model_.setRows(terminal_.historyRows(), warehouse);
    // A plate on every truck, or where a warehouse row went; one truck needs none.
    table_->setColumnHidden(HistoryTableModel::Place, place != "all" && !warehouse);
    if (model_.rowCount() > 0) {
        body_->setCurrentWidget(table_);
    } else {
        body_->setCurrentWidget(empty_);
        empty_->setTexts(!hasDay      ? tr("Ще не було жодної сесії")
                         : warehouse ? tr("Склад цього дня не змінювався")
                                     : tr("Ще нічого не завантажено"),
                         !hasDay ? tr("Історія з'явиться, щойно відкриють першу сесію й запишуть палету.")
                                 : tr("Тут з'являється кожна палета, щойно вона потрапляє у фуру чи на склад — "
                                      "з «Прийому» чи з «Розподілу»."));
    }
}

void HistoryScreen::resizeEvent(QResizeEvent* event) {
    // Down to the minimum window (1024) the supplier column still needs room.
    if (const bool narrow = width() < 1200; narrow != narrow_) {
        narrow_ = narrow;
        applyWidths();
    }
    QWidget::resizeEvent(event);
}

void HistoryScreen::applyWidths() {
    sidePanel_->setFixedWidth(narrow_ ? 210 : 260);
    // Column widths include the cell padding the delegate leaves (kSpacingM in all).
    const int widths[] = {92, narrow_ ? 90 : 110, 0, narrow_ ? 84 : 110, 44, 52, narrow_ ? 72 : 80, narrow_ ? 80 : 100};
    for (int column = 0; column < HistoryTableModel::ColumnCount; ++column) {
        if (column != HistoryTableModel::From) table_->setColumnWidth(column, widths[column] + kSpacingM);
    }
}

} // namespace spike
