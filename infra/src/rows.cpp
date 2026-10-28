#include "rows.h"

#include <optional>

namespace wms::rows {

std::int64_t toNanoseconds(TimePoint at) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(at.time_since_epoch()).count();
}

TimePoint fromNanoseconds(std::int64_t ns) {
    return TimePoint{std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds{ns})};
}

std::optional<std::int64_t> toNanoseconds(std::optional<TimePoint> at) {
    return at ? std::optional{toNanoseconds(*at)} : std::nullopt;
}

std::optional<TimePoint> optionalTime(const sqlite::Statement& row, int column) {
    return row.isNull(column) ? std::nullopt : std::optional{fromNanoseconds(row.columnInt(column))};
}

Uuid uuidColumn(const sqlite::Statement& row, int column) {
    const std::string text = row.columnText(column);
    const std::optional<Uuid> id = Uuid::parse(text);
    if (!id) throw std::runtime_error("Malformed UUID in database: " + text);
    return *id;
}

std::optional<Uuid> optionalUuidColumn(const sqlite::Statement& row, int column) {
    return row.isNull(column) ? std::nullopt : std::optional{uuidColumn(row, column)};
}

void bindOptionalUuid(sqlite::Statement& statement, int index, const std::optional<Uuid>& id) {
    if (id) {
        statement.bind(index, id->toString());
    } else {
        statement.bind(index, std::nullopt);
    }
}

void bindWeighing(sqlite::Statement& statement, int first, const Weighing& weighing) {
    statement.bind(first, weighing.cropCode)
        .bind(first + 1, weighing.gross.raw())
        .bind(first + 2, std::int64_t{weighing.boxCount})
        .bind(first + 3, weighing.tarePerBox.raw())
        .bind(first + 4, weighing.palletTare.raw())
        .bind(first + 5, weighing.pricePerKg.raw())
        .bind(first + 6, toText(kPriceSources, weighing.priceSource))
        .bind(first + 7, toText(kWeightSources, weighing.weightSource))
        .bind(first + 8, std::int64_t{weighing.cancelled ? 1 : 0});
}

Weighing readWeighing(const sqlite::Statement& row, int first) {
    return {
        .cropCode = row.columnText(first),
        .gross = grams(row.columnInt(first + 1)),
        .boxCount = static_cast<int>(row.columnInt(first + 2)),
        .tarePerBox = grams(row.columnInt(first + 3)),
        .palletTare = grams(row.columnInt(first + 4)),
        .pricePerKg = kopecks(row.columnInt(first + 5)),
        .priceSource = fromText(kPriceSources, row.columnText(first + 6)),
        .weightSource = fromText(kWeightSources, row.columnText(first + 7)),
        .cancelled = row.columnInt(first + 8) != 0,
    };
}

bool inserted(sqlite::Statement& insert) {
    const bool row = insert.step();
    while (insert.step()) {
    }
    return row;
}

} // namespace wms::rows
