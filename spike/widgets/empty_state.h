#pragma once

#include <QLabel>
#include <QWidget>

namespace spike {

// What a panel shows while it has nothing to show, as EmptyState.qml.
class EmptyState : public QWidget {
public:
    explicit EmptyState(QString icon, QWidget* parent = nullptr);

    void setTexts(const QString& title, const QString& text);

protected:
    // The icon is tinted with the theme's accent, which may change.
    void changeEvent(QEvent* event) override;

private:
    void paintBadge();

    QString icon_;
    QLabel* badge_;
    QLabel* title_;
    QLabel* text_;
};

} // namespace spike
