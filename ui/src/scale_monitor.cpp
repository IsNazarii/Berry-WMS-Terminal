#include "scale_monitor.h"

#include "display_format.h"

#include <chrono>

namespace wms::ui {
namespace {

using namespace std::chrono_literals;

constexpr auto kPollInterval = 250ms;

} // namespace

ScaleMonitor::ScaleMonitor(IScaleDriver& driver, QObject* parent)
    : QObject{parent}, driver_{&driver} {
    driver_->open();
    connect(&timer_, &QTimer::timeout, this, &ScaleMonitor::poll);
    timer_.start(kPollInterval);
}

QString ScaleMonitor::display() const {
    return reading_ ? kilogramsText(reading_->weight) : QStringLiteral("—");
}

std::optional<Weight> ScaleMonitor::stableWeight() const noexcept {
    if (!isStable()) return std::nullopt;
    return reading_->weight;
}

std::optional<Weight> ScaleMonitor::lastReading() const noexcept {
    if (!reading_) return std::nullopt;
    return reading_->weight;
}

void ScaleMonitor::setDriver(IScaleDriver& driver) {
    if (&driver == driver_) return;
    driver_->close();
    driver_ = &driver;
    driver_->open();
    show(std::nullopt);
    emit driverChanged();
}

void ScaleMonitor::poll() {
    // A driver that lost the connection gets reopened on the next tick.
    if (!driver_->isOpen() && !driver_->open()) return show(std::nullopt);
    const auto reading = driver_->read();
    if (reading) return show(reading);
    // Nothing new keeps the last reading; a lost connection must not, or a pallet
    // could be recorded with the weight shown before the cable came out.
    if (!driver_->isOpen()) show(std::nullopt);
}

void ScaleMonitor::show(const std::optional<ScaleReading>& reading) {
    // Most ticks repeat the same reading; redraw only when something visible changed.
    if (reading == reading_) return;
    reading_ = reading;
    emit readingChanged();
}

} // namespace wms::ui
