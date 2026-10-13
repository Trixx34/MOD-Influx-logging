#include "influx_settings.h"

#include <charconv>
#include <system_error>

namespace influx_stats {

namespace {

bool ParseNumber(std::string const& text, std::uint32_t& value)
{
    std::uint32_t parsed = 0;
    auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (error != std::errc() || end != text.data() + text.size() || text.empty())
        return false;
    value = parsed;
    return true;
}

void ReadNumber(std::unordered_map<std::string, std::string> const& config, std::string const& key, std::uint32_t minimum,
                std::uint32_t maximum, std::uint32_t& target, std::vector<std::string>& warnings)
{
    auto const it = config.find(key);
    if (it == config.end())
        return;
    std::uint32_t value = 0;
    if (ParseNumber(it->second, value) && value >= minimum && value <= maximum)
        target = value;
    else
        warnings.push_back("Module.influx-stats." + key + ": '" + it->second + "' is not a whole number from " +
                           std::to_string(minimum) + " to " + std::to_string(maximum) + "; using " + std::to_string(target) + ".");
}

void ReadString(std::unordered_map<std::string, std::string> const& config, std::string const& key, std::string& target)
{
    if (auto const it = config.find(key); it != config.end())
        target = it->second;
}

std::string Trim(std::string text)
{
    text.erase(0, text.find_first_not_of(' '));
    text.erase(text.find_last_not_of(' ') + 1);
    return text;
}

// http://host[:port][/]. Returns false, with the reason, for anything else.
bool ParseUrl(std::string const& url, std::string& host, std::uint16_t& port, std::string& why)
{
    std::string const scheme = "http://";
    if (url.rfind("https://", 0) == 0)
    {
        why = "https is not supported; point Url at a local http listener (InfluxDB on the same host or a reverse proxy)";
        return false;
    }
    if (url.rfind(scheme, 0) != 0)
    {
        why = "must start with http://";
        return false;
    }
    std::string rest = url.substr(scheme.size());
    if (!rest.empty() && rest.back() == '/')
        rest.pop_back();
    std::size_t const colon = rest.rfind(':');
    std::string hostPart = colon == std::string::npos ? rest : rest.substr(0, colon);
    std::uint32_t parsedPort = 80;
    if (colon != std::string::npos && (!ParseNumber(rest.substr(colon + 1), parsedPort) || parsedPort == 0 || parsedPort > 65535))
    {
        why = "the port is not a number from 1 to 65535";
        return false;
    }
    if (hostPart.empty() || hostPart.find('/') != std::string::npos)
    {
        why = "needs a host name or address and no path";
        return false;
    }
    host = hostPart;
    port = static_cast<std::uint16_t>(parsedPort);
    return true;
}

} // namespace

Settings ParseSettings(std::unordered_map<std::string, std::string> const& config, std::vector<std::string>& warnings)
{
    Settings settings;

    if (auto const it = config.find("Enabled"); it != config.end())
    {
        if (it->second == "0" || it->second == "1")
            settings.Enabled = it->second == "1";
        else
            warnings.push_back("Module.influx-stats.Enabled: '" + it->second + "' is not 0 or 1; using 0.");
    }

    if (auto const it = config.find("Url"); it != config.end())
    {
        std::string host;
        std::uint16_t port = 0;
        std::string why;
        if (ParseUrl(Trim(it->second), host, port, why))
        {
            settings.Host = host;
            settings.Port = port;
        }
        else
            warnings.push_back("Module.influx-stats.Url: '" + it->second + "' " + why + "; using http://127.0.0.1:8086.");
    }

    ReadString(config, "Org", settings.Org);
    ReadString(config, "Bucket", settings.Bucket);
    ReadString(config, "Token", settings.Token);
    ReadString(config, "MeasurementPrefix", settings.MeasurementPrefix);

    ReadNumber(config, "FlushIntervalMs", 100, 60000, settings.FlushIntervalMs, warnings);
    ReadNumber(config, "BatchSize", 1, 5000, settings.BatchSize, warnings);
    ReadNumber(config, "MaxQueue", settings.BatchSize, 1000000, settings.MaxQueue, warnings);
    ReadNumber(config, "RequestTimeoutMs", 100, 60000, settings.RequestTimeoutMs, warnings);

    if (auto const it = config.find("GlobalTags"); it != config.end())
    {
        std::string const& list = it->second;
        std::size_t start = 0;
        while (start <= list.size())
        {
            std::size_t comma = list.find(',', start);
            if (comma == std::string::npos)
                comma = list.size();
            std::string const entry = Trim(list.substr(start, comma - start));
            start = comma + 1;
            if (entry.empty())
                continue;
            std::size_t const equals = entry.find('=');
            std::string const key = equals == std::string::npos ? "" : Trim(entry.substr(0, equals));
            std::string const value = equals == std::string::npos ? "" : Trim(entry.substr(equals + 1));
            if (key.empty() || value.empty())
                warnings.push_back("Module.influx-stats.GlobalTags: '" + entry + "' is not key=value; ignored.");
            else
                settings.GlobalTags.emplace_back(key, value);
        }
    }

    return settings;
}

std::string ValidateForWriting(Settings const& settings)
{
    if (settings.Org.empty())
        return "Module.influx-stats.Org is empty";
    if (settings.Bucket.empty())
        return "Module.influx-stats.Bucket is empty";
    if (settings.Token.empty())
        return "Module.influx-stats.Token is empty";
    return {};
}

} // namespace influx_stats
