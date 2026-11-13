#include "empty_state.h"

#include "theme.h"

#include <QEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace spike {

EmptyState::EmptyState(QString icon, QWidget* parent)
    : QWidget{parent}, icon_{std::move(icon)}, badge_{new QLabel}, title_{new QLabel}, text_{new QLabel} {
    title_->setProperty("role", "emptyTitle");
    text_->setProperty("role", "emptyText");
    for (QLabel* label : {badge_, title_, text_}) label->setAlignment(Qt::AlignHCenter);
    title_->setWordWrap(true);
    text_->setWordWrap(true);
    text_->setMaximumWidth(520 - kSpacingXl * 4);

    auto* layout = new QVBoxLayout{this};
    layout->setSpacing(kSpacingM);
    layout->addStretch();
    layout->addWidget(badge_, 0, Qt::AlignHCenter);
    layout->addWidget(title_);
    layout->addWidget(text_, 0, Qt::AlignHCenter);
    layout->addStretch();
    paintBadge();
}

void EmptyState::setTexts(const QString& title, const QString& text) {
    title_->setText(title);
    text_->setText(text);
}

void EmptyState::changeEvent(QEvent* event) {
    if (event->type() == QEvent::StyleChange) paintBadge();
    QWidget::changeEvent(event);
}

void EmptyState::paintBadge() {
    // A round soft-accent badge with the icon in the middle.
    QPixmap badge{kEmptyBadgeSize, kEmptyBadgeSize};
    badge.fill(Qt::transparent);
    QPainter painter{&badge};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme().accentSoft);
    painter.drawEllipse(badge.rect());
    const int offset = (kEmptyBadgeSize - kIconSizeLarge) / 2;
    painter.drawPixmap(offset, offset, tintedIcon(icon_, kIconSizeLarge, theme().accent));
    painter.end();
    badge_->setPixmap(badge);
}

} // namespace spike
