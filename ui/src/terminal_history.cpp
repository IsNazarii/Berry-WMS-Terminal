// The history screen's part of Terminal: what a work day loaded and where from.
// Kept apart from terminal.cpp only to keep that file readable; same class.
#include "terminal.h"

#include "display_format.h"

#include <QDateTime>

#include <algorithm>
#include <chrono>
#include <format>
#include <iterator>
#include <ranges>

namespace wms::ui {

namespace {

constexpr auto kAllTrucks = "all";
constexpr auto kWarehouse = "warehouse";

QDateTime localDateTimeOf(TimePoint at) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(at.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms);
}

// Within the day only the time; a row from outside it (yesterday's pallet in a
// carried-over truck) gets its date too, so it does not pass for today's.
QString historyTime(TimePoint at, const WorkDay& day) {
    const bool inDay = at >= day.openedAt() && (!day.closedAt() || at <= *day.closedAt());
    return localDateTimeOf(at).toString(inDay ? "HH:mm" : "dd.MM HH:mm");
}

QString dayTitle(std::chrono::year_month_day date) {
    return QString::fromStdString(std::format("Сесія {:%d.%m.%Y}", date));
}

QString cargoText(const Cargo& cargo) {
    return Terminal::tr("%1 ящ. · %2 кг").arg(cargo.boxes).arg(kilogramsText(cargo.weight));
}

} // namespace

const WorkDay* Terminal::historyDay() const noexcept {
    // Without a choice the open session wins, as historySessionId documents.
    const std::optional<Uuid> target = historySessionId_ ? historySessionId_
                                       : workDay_         ? std::optional{workDay_->id()}
                                                          : std::nullopt;
    if (workDay_ && target == workDay_->id()) return &*workDay_;
    if (historyArchive_ && (!target || target == historyArchive_->id())) return &*historyArchive_;
    return nullptr;
}

void Terminal::refreshHistoryDay() {
    if (historySessionId_ && !(workDay_ && workDay_->id() == *historySessionId_)) {
        historyArchive_ = sessionStore_.find(*historySessionId_);
        // Gone from the store (cannot happen today, but sync may change that): back to the default.
        if (!historyArchive_) historySessionId_.reset();
    }
    if (!historySessionId_) {
        // The default day: the open session, else the archive's newest.
        historyArchive_ = workDay_ ? std::nullopt : sessionStore_.lastClosed();
    }
    emit historyChanged();
}

QVariantList Terminal::historySessions() const {
    QVariantList list;
    if (workDay_) {
        list.append(QVariantMap{
            {"sessionId", QString::fromStdString(workDay_->id().toString())},
            {"title", dayTitle(workDay_->date())},
            {"detail", tr("відкрита")},
        });
    }
    for (const ClosedSessionInfo& closed : sessionStore_.closedSessions()) {
        list.append(QVariantMap{
            {"sessionId", QString::fromStdString(closed.id.toString())},
            {"title", dayTitle(closed.date)},
            {"detail", tr("%1 – %2").arg(localDateTimeOf(closed.openedAt).toString("dd.MM HH:mm"),
                                         localDateTimeOf(closed.closedAt).toString("dd.MM HH:mm"))},
        });
    }
    return list;
}

QString Terminal::historySessionId() const {
    const WorkDay* day = historyDay();
    return day ? QString::fromStdString(day->id().toString()) : QString{};
}

QVariantList Terminal::historyPlaces() const {
    const WorkDay* day = historyDay();
    if (!day) return {};
    const LoadingHistory history{*day, journal_, workDay_ ? workDay_->openReceipts() : std::span<const OpenReceipt>{}};

    QVariantList list;
    list.append(QVariantMap{
        {"placeId", QString(kAllTrucks)},
        {"title", tr("Усі фури")},
        {"detail", tr("фур: %1").arg(day->trucks().size())},
        {"rows", static_cast<int>(history.intoTrucks().size())},
    });
    for (const Truck& truck : day->trucks()) {
        const Cargo load = journal_.loadOf(truck.id());
        list.append(QVariantMap{
            {"placeId", QString::fromStdString(truck.id().toString())},
            {"title", QString::fromStdString(truck.plate())},
            {"detail", tr("%1 / %2 ящ.").arg(load.boxes).arg(truck.boxLimit())},
            {"rows", static_cast<int>(history.intoTruck(truck.id()).size())},
        });
    }
    list.append(QVariantMap{
        {"placeId", QString(kWarehouse)},
        {"title", tr("Склад")},
        {"detail", tr("прихід і відвантаження")},
        {"rows", static_cast<int>(history.warehouse().size())},
    });
    return list;
}

QString Terminal::historyPlace() const {
    // A truck of another day falls back to every truck, as selectHistoryPlace() would.
    const WorkDay* day = historyDay();
    if (historyPlace_ == kAllTrucks || historyPlace_ == kWarehouse || !day) return historyPlace_;
    const auto id = Uuid::parse(historyPlace_.toStdString());
    return id && day->findTruck(*id) ? historyPlace_ : QString(kAllTrucks);
}

std::vector<LoadingEntry> Terminal::historyEntries() const {
    const WorkDay* day = historyDay();
    if (!day) return {};
    const LoadingHistory history{*day, journal_, workDay_ ? workDay_->openReceipts() : std::span<const OpenReceipt>{}};
    const QString place = historyPlace();
    if (place == kAllTrucks) return history.intoTrucks();
    if (place == kWarehouse) return history.warehouse();
    return history.intoTruck(*Uuid::parse(place.toStdString()));
}

QString Terminal::historyPlateOf(const TruckId& id) const {
    if (const WorkDay* day = historyDay()) {
        if (const Truck* truck = day->findTruck(id)) return QString::fromStdString(truck->plate());
    }
    return plateOf(id);
}

QVariantList Terminal::historyRows() const {
    const WorkDay* day = historyDay();
    if (!day) return {};
    QVariantList rows;
    for (const LoadingEntry& e : historyEntries()) {
        const auto* truckId = std::get_if<TruckId>(&e.to);
        QString reversedNote;
        if (e.reversedAt) {
            reversedNote = tr("знято %1").arg(historyTime(*e.reversedAt, *day));
            if (e.reversedBy) reversedNote += " · " + operatorNameOf(*e.reversedBy);
        }
        rows.append(QVariantMap{
            {"time", historyTime(e.at, *day)},
            {"place", truckId ? historyPlateOf(*truckId) : tr("склад")},
            {"from", e.fromWarehouse() ? tr("зі складу")
                     : e.supplierId    ? supplierNameOf(*e.supplierId)
                                       : tr("(чек невідомий)")},
            {"crop", cropName(e.cropCode)},
            // Boxes counted out of the receipt are not one pallet.
            {"pallet", e.weighingIndex ? QString::number(*e.weighingIndex + 1) : QString("—")},
            {"boxes", QString::number(e.cargo.boxes)},
            {"kg", kilogramsText(e.cargo.weight)},
            {"by", e.by ? operatorNameOf(*e.by) : QString("—")},
            {"reversed", e.reversed()},
            {"reversedNote", reversedNote},
            {"out", e.fromWarehouse()},
        });
    }
    return rows;
}

QString Terminal::historySummary() const {
    const std::vector<LoadingEntry> entries = historyEntries();
    Cargo in;
    Cargo out;
    int pallets = 0;
    for (const LoadingEntry& e : entries) {
        if (e.reversed()) continue;
        // On the warehouse view a shipment leaves it; on a truck view it arrives.
        if (historyPlace() == kWarehouse && e.fromWarehouse()) {
            out += e.cargo;
        } else {
            in += e.cargo;
            if (e.weighingIndex) ++pallets;
        }
    }
    if (entries.empty()) return {};
    if (historyPlace() == kWarehouse) {
        return tr("надійшло %1 · відвантажено %2").arg(cargoText(in), cargoText(out));
    }
    return tr("%1 · %2").arg(palletsText(pallets), cargoText(in));
}

void Terminal::selectHistorySession(const QString& sessionId) {
    const auto id = Uuid::parse(sessionId.toStdString());
    historySessionId_ = id;
    refreshHistoryDay();
}

void Terminal::selectHistoryPlace(const QString& placeId) {
    historyPlace_ = placeId;
    emit historyChanged();
}

} // namespace wms::ui
