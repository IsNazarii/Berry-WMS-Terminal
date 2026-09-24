#pragma once

#include "wms/quantity.h"
#include "wms/uuid.h"

#include <chrono>
#include <string>
#include <vector>

namespace wms {

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

enum class SyncStatus { Pending, Synced };
enum class TransactionKind { Receipt, Payment };

// Base of every record that is written to the local offline buffer and later
// synchronised with the server. Records are immutable facts: mistakes are
// corrected by a new reversing record, never by editing an existing one.
class Transaction {
public:
    virtual ~Transaction() = default;

    [[nodiscard]] const Uuid& id() const noexcept { return id_; }
    [[nodiscard]] TimePoint createdAt() const noexcept { return createdAt_; }
    [[nodiscard]] SyncStatus syncStatus() const noexcept { return syncStatus_; }
    void markSynced() noexcept { syncStatus_ = SyncStatus::Synced; }

    [[nodiscard]] virtual TransactionKind kind() const noexcept = 0;
    // How the record changes what we owe the supplier: positive = we owe more.
    [[nodiscard]] virtual Money balanceEffect() const noexcept = 0;
    // Human-readable validation errors; empty when the record is valid.
    [[nodiscard]] virtual std::vector<std::string> validate() const = 0;
    [[nodiscard]] virtual std::string describe() const = 0;

    [[nodiscard]] bool isValid() const { return validate().empty(); }

protected:
    Transaction(Uuid id, TimePoint createdAt, SyncStatus syncStatus);

    // Copy only through derived classes, so a Transaction& cannot be sliced.
    Transaction(const Transaction&) = default;
    Transaction& operator=(const Transaction&) = default;

private:
    Uuid id_;
    TimePoint createdAt_;
    SyncStatus syncStatus_;
};

} // namespace wms
