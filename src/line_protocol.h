// InfluxDB line protocol encoding. No SDK, core or network needed (tests/line_protocol_test.cpp).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace influx_stats {

using FieldValue = std::variant<std::int64_t, double, bool, std::string>;

struct Point
{
    std::string Measurement;
    std::vector<std::pair<std::string, std::string>> Tags;
    std::vector<std::pair<std::string, FieldValue>> Fields;
    std::int64_t TimestampNs = 0; // 0 leaves the timestamp out and lets InfluxDB stamp the write
};

// One line, without the trailing newline: measurement,tag=v field=v timestamp. Tags are sorted by key (InfluxDB's preferred order).
// Integers carry the 'i' suffix, strings are quoted, and special characters are escaped. A point with no fields or an empty
// measurement cannot be written: the result is empty.
std::string EncodePoint(Point point);

// Points joined by newlines, skipping any that encode to nothing.
std::string EncodeBatch(std::vector<Point> const& points);

} // namespace influx_stats
