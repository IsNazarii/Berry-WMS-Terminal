#include "delegates.h"

#include "theme.h"

#include <QEvent>
#include <QPainter>
#include <QTableView>

namespace spike {

namespace {

QFont fontOf(const QStyleOptionViewItem& option, int pixelSize, QFont::Weight weight) {
    QFont font = option.font;
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

// Draws one line of text, elided to fit.
void drawText(QPainter* painter, const QRect& rect, const QString& text, const QFont& font, const QColor& color,
              Qt::Alignment alignment) {
    painter->setFont(font);
    painter->setPen(color);
    const QString elided = QFontMetrics{font}.elidedText(text, Qt::ElideRight, rect.width());
    painter->drawText(rect, int(alignment | Qt::AlignVCenter), elided);
}

} // namespace

void ListRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const Theme& t = theme();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRect box = option.rect.adjusted(0, 0, 0, -kSpacingXs); // the gap between rows
    if (selected || hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? t.accentSoft : t.surfaceAlt);
        painter->drawRoundedRect(box, kRadiusS, kRadiusS);
    }
    if (selected) {
        painter->setBrush(t.accent);
        painter->drawRoundedRect(QRect{box.left(), box.top(), kSelectionBarWidth, box.height()}, 1.5, 1.5);
    }

    const QRect content = box.adjusted(kSpacingM, kSpacingS, -kSpacingM, -kSpacingS);
    const QString trailing = index.data(TrailingRole).toString();
    const QFont small = fontOf(option, kFontCaption, QFont::Normal);
    const int trailingWidth = trailing.isEmpty() ? 0 : QFontMetrics{small}.horizontalAdvance(trailing) + kSpacingS;
    const QRect text = content.adjusted(0, 0, -trailingWidth, 0);
    const int half = text.height() / 2;
    drawText(painter, text.adjusted(0, 0, 0, -half), index.data(TitleRole).toString(),
             fontOf(option, kFontBody, QFont::DemiBold), t.text, Qt::AlignLeft);
    drawText(painter, text.adjusted(0, half, 0, 0), index.data(DetailRole).toString(), small, t.textMuted,
             Qt::AlignLeft);
    if (!trailing.isEmpty()) drawText(painter, content, trailing, small, t.text, Qt::AlignRight);
    painter->restore();
}

QSize ListRowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    return {option.rect.width(), 58};
}

TableCellDelegate::TableCellDelegate(QTableView* view) : QStyledItemDelegate{view}, view_{view} {
    view->setMouseTracking(true);
    QObject::connect(view, &QTableView::entered, view, [this, view](const QModelIndex& index) {
        hoveredRow_ = index.row();
        view->viewport()->update();
    });
    // Leaving the table clears the highlight; `entered` alone never says so.
    view->viewport()->installEventFilter(this);
    QObject::connect(view->model(), &QAbstractItemModel::modelReset, view, [this] { hoveredRow_ = -1; });
}

bool TableCellDelegate::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Leave) {
        hoveredRow_ = -1;
        view_->viewport()->update();
    }
    return QStyledItemDelegate::eventFilter(watched, event);
}

void TableCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const Theme& t = theme();
    painter->save();
    if (index.row() == hoveredRow_) painter->fillRect(option.rect, t.surfaceAlt);
    painter->fillRect(QRect{option.rect.left(), option.rect.bottom(), option.rect.width(), 1}, t.surfaceAlt);

    const bool struck = index.data(StruckRole).toBool();
    QFont font = fontOf(option, kFontBody, index.data(BoldRole).toBool() ? QFont::DemiBold : QFont::Normal);
    font.setStrikeOut(struck);
    const QColor color = struck || index.data(MutedRole).toBool() ? t.textMuted : t.text;
    const auto alignment = Qt::Alignment(index.data(Qt::TextAlignmentRole).toInt()) & Qt::AlignHorizontal_Mask;
    const QRect cell = option.rect.adjusted(kSpacingM / 2, 0, -kSpacingM / 2, 0);

    const QString note = index.data(NoteRole).toString();
    if (note.isEmpty()) {
        drawText(painter, cell, index.data().toString(), font, color, alignment);
    } else {
        const int half = cell.height() / 2;
        drawText(painter, cell.adjusted(0, kSpacingXs, 0, -half + kSpacingXs), index.data().toString(), font, color,
                 alignment);
        drawText(painter, cell.adjusted(0, half - kSpacingXs, 0, -kSpacingXs), note,
                 fontOf(option, kFontCaption, QFont::Normal), t.warn, alignment);
    }
    painter->restore();
}

} // namespace spike
