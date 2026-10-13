// Reads the module's settings from the config entries the core sends (Module.influx-stats.<Key>, prefix removed).
// A bad value keeps the default and adds a warning. No SDK or core needed (tests/settings_test.cpp).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace influx_stats {

struct Settings
{
    bool Enabled = false;
    std::string Host = "127.0.0.1";
    std::uint16_t Port = 8086;
    std::string Org;
    std::string Bucket = "forever";
    std::string Token;
    std::uint32_t FlushIntervalMs = 5000;
    std::uint32_t BatchSize = 500;
    std::uint32_t MaxQueue = 10000;
    std::uint32_t RequestTimeoutMs = 3000;
    std::string MeasurementPrefix = "forever_";
    std::vector<std::pair<std::string, std::string>> GlobalTags;
};

// Keys: Enabled (0/1, default 0), Url (http://host[:port], default http://127.0.0.1:8086; https is refused), Org, Bucket (default
// forever), Token, FlushIntervalMs (100..60000, default 5000), BatchSize (1..5000, default
// 500), MaxQueue (BatchSize..1000000, default 10000), RequestTimeoutMs (100..60000, default 3000), MeasurementPrefix (default
// "forever_"), GlobalTags (comma-separated key=value pairs, default none).
Settings ParseSettings(std::unordered_map<std::string, std::string> const& config, std::vector<std::string>& warnings);

// Empty when the settings can be used to write; otherwise why not. Only checked when Enabled is on.
std::string ValidateForWriting(Settings const& settings);

} // namespace influx_stats
