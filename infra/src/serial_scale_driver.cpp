#include "wms/serial_scale_driver.h"

#include <QByteArray>

#include <string_view>

namespace wms {

SerialScaleDriver::SerialScaleDriver(const QString& portName, std::chrono::milliseconds silenceLimit)
    : silenceLimit_{silenceLimit} {
    port_.setPortName(portName);
    // What the scale on the point is set to (F2 = 1, F3 = 1); see docs/hardware/scale-ie04.md.
    port_.setBaudRate(QSerialPort::Baud4800);
    port_.setDataBits(QSerialPort::Data8);
    port_.setParity(QSerialPort::NoParity);
    port_.setStopBits(QSerialPort::OneStop);
    port_.setFlowControl(QSerialPort::NoFlowControl);
}

bool SerialScaleDriver::open() {
    if (port_.isOpen()) return true;
    if (!port_.open(QIODevice::ReadOnly)) return false;
    // Bytes left from before the reconnect belong to frames that are long gone.
    reader_ = {};
    sinceReading_.start();
    return true;
}

void SerialScaleDriver::close() {
    port_.close();
    port_.clearError();
}

std::optional<ScaleReading> SerialScaleDriver::read() {
    if (!port_.isOpen()) return std::nullopt;

    const QByteArray chunk = port_.readAll();
    auto reading = reader_.push(std::string_view{chunk.constData(), static_cast<std::size_t>(chunk.size())});
    if (reading) sinceReading_.restart();

    // An unplugged adapter shows up as an error. A switched-off scale is silent, and
    // one showing an error code sends frames without a weight: both are "no weight",
    // since a working scale sends ten weights a second.
    const bool unplugged = port_.error() == QSerialPort::ResourceError;
    const bool noWeight = !reading && sinceReading_.durationElapsed() > silenceLimit_;
    if (unplugged || noWeight) {
        close();
        return std::nullopt;
    }
    return reading;
}

} // namespace wms
