#pragma once

#include <vector>

namespace wms {

// Reference data (suppliers, operators, crops) between runs. Unlike the journal it
// is not a log: the server owns these records and sends a newer version of a
// record whole, so saving an id that is already stored replaces it.
// One interface for every kind of record: Repository<Supplier> and
// Repository<Operator> are unrelated types made from the same template.
template <class T>
class Repository {
public:
    virtual ~Repository() = default;

    // Adds the record, or replaces the stored one with the same id.
    virtual void save(const T& record) = 0;
    // Deletes the record with the same id; nothing happens when there is none.
    virtual void remove(const T& record) = 0;
    // Every record: in id order; crops, which have no number, in the order added.
    [[nodiscard]] virtual std::vector<T> all() const = 0;

protected:
    Repository() = default;
    Repository(const Repository&) = default;
    Repository& operator=(const Repository&) = default;
};

} // namespace wms
