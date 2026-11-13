#pragma once

#include <QModelIndex>
#include <QStyledItemDelegate>

class QTableView;

// Shared painting for lists and tables in the look of the QML screens.
namespace spike {

// Roles a model gives on top of DisplayRole / TextAlignmentRole.
enum Role {
    TitleRole = Qt::UserRole + 1, // list row: bold first line (DisplayRole is the title too)
    DetailRole,                   // list row: muted second line
    TrailingRole,                 // list row: small text on the right
    StruckRole,                   // table cell: crossed out and faded (bool)
    MutedRole,                    // table cell: muted color (bool)
    BoldRole,                     // table cell: semibold (bool)
    NoteRole,                     // table cell: a small warning line under the text
};

// A row of a side list: title, detail, a number on the right; the selected one
// has the soft accent and a bar on the left, as ItemDelegate.qml.
class ListRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

// A table cell: the hovered row highlighted across all columns, a hairline
// under each row, padding, elision and the roles above.
class TableCellDelegate : public QStyledItemDelegate {
public:
    // Tracks the hovered row of `view` (mouse tracking on).
    explicit TableCellDelegate(QTableView* view);
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTableView* view_;
    int hoveredRow_ = -1;
};

} // namespace spike
