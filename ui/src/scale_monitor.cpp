#include "scale_monitor.h"

#include <chrono>

namespace wms::ui {
namespace {

using namespace std::chrono_literals;

// Industrial scales step by 200 or 500 g, so a settled weight repeats exactly:
// three equal readings at 4 per second settle a pallet in under a second.
// A weight that keeps flipping between two steps is confirmed by the operator instead.
constexpr std::size_t kStableReadings = 3;
constexpr Weight kStableTolerance = grams(0);
constexpr auto kPollInterval = 250ms;

} // namespace

ScaleMonitor::ScaleMonitor(IScaleDriver& driver, QObject* parent)
    : QObject{parent}, driver_{driver}, detector_{kStableReadings, kStableTolerance} {
    driver_.open();
    connect(&timer_, &QTimer::timeout, this, &ScaleMonitor::poll);
    timer_.start(kPollInterval);
}

QString ScaleMonitor::display() const {
    const auto reading = detector_.lastReading();
    return reading ? QString::fromStdString(formatWeight(*reading)) : QStringLiteral("—");
}

void ScaleMonitor::poll() {
    // A driver that lost the connection gets reopened on the next tick.
    if (!driver_.isOpen() && !driver_.open()) return;
    const auto reading = driver_.read();
    if (!reading) return;

    const auto before = detector_.lastReading();
    const bool wasStable = detector_.isStable();
    detector_.push(*reading);
    // Most ticks repeat the same weight; redraw only when something visible changed.
    if (detector_.lastReading() != before || detector_.isStable() != wasStable) emit readingChanged();
}

} // namespace wms::ui
