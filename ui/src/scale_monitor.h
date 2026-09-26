#pragma once

#include "wms/scale.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace wms::ui {

// Polls a scale driver and runs the readings through the stability detector.
// Knows the driver only as IScaleDriver, so the simulator and the serial-port
// driver are interchangeable.
class ScaleMonitor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Terminal")

    Q_PROPERTY(QString display READ display NOTIFY readingChanged)
    Q_PROPERTY(bool stable READ isStable NOTIFY readingChanged)
    Q_PROPERTY(QString driverName READ driverName CONSTANT)

public:
    // The driver must outlive the monitor.
    explicit ScaleMonitor(IScaleDriver& driver, QObject* parent = nullptr);

    [[nodiscard]] QString display() const;
    [[nodiscard]] bool isStable() const noexcept { return detector_.isStable(); }
    [[nodiscard]] QString driverName() const { return QString::fromStdString(driver_.name()); }
    // The only weight a weighing may be recorded with.
    [[nodiscard]] std::optional<Weight> stableWeight() const noexcept { return detector_.stableWeight(); }
    // What the scale shows now, settled or not: the operator may accept it anyway.
    [[nodiscard]] std::optional<Weight> lastReading() const noexcept { return detector_.lastReading(); }

signals:
    void readingChanged();

private:
    void poll();

    IScaleDriver& driver_;
    StabilityDetector detector_;
    QTimer timer_;
};

} // namespace wms::ui
