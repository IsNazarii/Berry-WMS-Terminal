#include "wms/transaction.h"

#include <stdexcept>

namespace wms {

Transaction::Transaction(Uuid id, TimePoint createdAt, SyncStatus syncStatus)
    : id_{id}, createdAt_{createdAt}, syncStatus_{syncStatus} {
    if (id_.isNil()) throw std::invalid_argument("Transaction id must not be nil");
}

} // namespace wms
