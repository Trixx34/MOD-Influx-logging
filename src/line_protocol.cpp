#include "line_protocol.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace influx_stats {

namespace {

// Measurement names escape comma and space; tag keys, tag values and field keys also escape '='.
std::string Escape(std::string const& text, bool escapeEquals)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        if (c == ',' || c == ' ' || (escapeEquals && c == '='))
            out += '\\';
        // A newline would end the line, so it is replaced rather than escaped.
        out += (c == '\n' || c == '\r') ? ' ' : c;
    }
    return out;
}

std::string EscapeString(std::string const& text)
{
    std::string out = "\"";
    for (char c : text)
    {
        if (c == '"' || c == '\\')
            out += '\\';
        out += (c == '\n' || c == '\r') ? ' ' : c;
    }
    out += '"';
    return out;
}

bool AppendField(std::string& out, FieldValue const& value)
{
    if (auto const* i = std::get_if<std::int64_t>(&value))
        out += std::to_string(*i) + "i";
    else if (auto const* d = std::get_if<double>(&value))
    {
        if (!std::isfinite(*d))
            return false; // InfluxDB rejects NaN and infinity
        char buffer[64];
        auto const result = std::to_chars(buffer, buffer + sizeof(buffer), *d);
        out.append(buffer, result.ptr);
    }
    else if (auto const* b = std::get_if<bool>(&value))
        out += *b ? "true" : "false";
    else
        out += EscapeString(std::get<std::string>(value));
    return true;
}

} // namespace

std::string EncodePoint(Point point)
{
    if (point.Measurement.empty() || point.Fields.empty())
        return {};

    std::sort(point.Tags.begin(), point.Tags.end());

    std::string line = Escape(point.Measurement, false);
    for (auto const& [key, value] : point.Tags)
    {
        if (key.empty() || value.empty())
            continue; // InfluxDB rejects empty tag keys and values
        line += ',' + Escape(key, true) + '=' + Escape(value, true);
    }

    bool first = true;
    for (auto const& [key, value] : point.Fields)
    {
        std::string field;
        if (key.empty() || !AppendField(field, value))
            continue;
        line += (first ? ' ' : ',') + Escape(key, true) + '=' + field;
        first = false;
    }
    if (first)
        return {};

    if (point.TimestampNs != 0)
        line += ' ' + std::to_string(point.TimestampNs);
    return line;
}

std::string EncodeBatch(std::vector<Point> const& points)
{
    std::string body;
    for (Point const& point : points)
    {
        std::string line = EncodePoint(point);
        if (line.empty())
            continue;
        if (!body.empty())
            body += '\n';
        body += line;
    }
    return body;
}

} // namespace influx_stats
