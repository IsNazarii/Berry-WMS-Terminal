#pragma once

#include "wms/scale.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace wms::ui {

// Polls a scale driver and keeps its latest reading for the screens.
// Knows the driver only as IScaleDriver, so the simulator and the serial-port
// driver are interchangeable; stability is the driver's call.
class ScaleMonitor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Terminal")

    // The number only ("96,4"); the unit is a separate label on the indicator.
    Q_PROPERTY(QString display READ display NOTIFY readingChanged)
    Q_PROPERTY(bool zero READ isZero NOTIFY readingChanged)
    Q_PROPERTY(bool stable READ isStable NOTIFY readingChanged)
    // The indicator's own tare is on: the display shows net weight.
    Q_PROPERTY(bool tare READ isTare NOTIFY readingChanged)
    // There is a weight to show: the scale is connected and its display shows a number.
    Q_PROPERTY(bool online READ isOnline NOTIFY readingChanged)
    Q_PROPERTY(QString driverName READ driverName CONSTANT)

public:
    // The driver must outlive the monitor.
    explicit ScaleMonitor(IScaleDriver& driver, QObject* parent = nullptr);

    [[nodiscard]] QString display() const;
    [[nodiscard]] bool isStable() const noexcept { return reading_ && reading_->stable; }
    // An empty scale: the "zero" lamp of a real indicator.
    [[nodiscard]] bool isZero() const noexcept { return reading_ && reading_->zero; }
    [[nodiscard]] bool isTare() const noexcept { return reading_ && reading_->tare; }
    [[nodiscard]] bool isOnline() const noexcept { return reading_.has_value(); }
    [[nodiscard]] QString driverName() const { return QString::fromStdString(driver_.name()); }
    // The only weight a weighing may be recorded with.
    [[nodiscard]] std::optional<Weight> stableWeight() const noexcept;
    // What the scale shows now, settled or not: the operator may accept it anyway.
    [[nodiscard]] std::optional<Weight> lastReading() const noexcept;

signals:
    void readingChanged();

private:
    void poll();
    void show(const std::optional<ScaleReading>& reading);

    IScaleDriver& driver_;
    std::optional<ScaleReading> reading_;
    QTimer timer_;
};

} // namespace wms::ui
