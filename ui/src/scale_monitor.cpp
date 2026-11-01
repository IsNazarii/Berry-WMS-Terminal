#include "scale_monitor.h"

#include "display_format.h"

#include <chrono>

namespace wms::ui {
namespace {

using namespace std::chrono_literals;

constexpr auto kPollInterval = 250ms;

} // namespace

ScaleMonitor::ScaleMonitor(IScaleDriver& driver, QObject* parent)
    : QObject{parent}, driver_{driver} {
    driver_.open();
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

void ScaleMonitor::poll() {
    // A driver that lost the connection gets reopened on the next tick.
    if (!driver_.isOpen() && !driver_.open()) return;
    const auto reading = driver_.read();
    if (!reading) return;

    // Most ticks repeat the same reading; redraw only when something visible changed.
    if (reading == reading_) return;
    reading_ = reading;
    emit readingChanged();
}

} // namespace wms::ui
