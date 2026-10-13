// Unit tests for ParseSettings. No core needed.
#include "influx_settings.h"

#include <cstdio>
namespace {
int failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            ++failures;                                                \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                              \
    } while (0)
}

int main()
{
    using namespace influx_stats;
    {
        std::vector<std::string> w;
        Settings s = ParseSettings({}, w);
        CHECK(!s.Enabled && s.Host == "127.0.0.1" && s.Port == 8086 && s.Bucket == "forever" && s.BatchSize == 500);
        CHECK(s.FlushIntervalMs == 5000 && s.MaxQueue == 10000 && s.MeasurementPrefix == "forever_");
        CHECK(w.empty());
        CHECK(!ValidateForWriting(s).empty());
    }
    {
        std::vector<std::string> w;
        Settings s = ParseSettings({{"Enabled", "1"}, {"Url", "http://db.local:9000/"}, {"Org", "o"}, {"Bucket", "b"}, {"Token", "t"},
                                    {"GlobalTags", "realm=sim1, host=alpha"}, {"MeasurementPrefix", ""}}, w);
        CHECK(s.Enabled && s.Host == "db.local" && s.Port == 9000 && s.MeasurementPrefix.empty());
        CHECK(s.GlobalTags.size() == 2 && s.GlobalTags[1].first == "host" && s.GlobalTags[1].second == "alpha");
        CHECK(ValidateForWriting(s).empty() && w.empty());
    }
    {
        std::vector<std::string> w;
        Settings s = ParseSettings({{"Url", "http://db.local"}}, w);
        CHECK(s.Host == "db.local" && s.Port == 80 && w.empty());
    }
    {
        std::vector<std::string> w;
        Settings s = ParseSettings({{"Url", "https://db.local:8086"}, {"Enabled", "yes"}, {"BatchSize", "0"},
                                    {"GlobalTags", "novalue,=x"}}, w);
        CHECK(s.Host == "127.0.0.1" && !s.Enabled && s.BatchSize == 500 && s.GlobalTags.empty());
        CHECK(w.size() == 5);
    }
    {
        std::vector<std::string> w;
        Settings s = ParseSettings({{"BatchSize", "100"}, {"MaxQueue", "50"}}, w);
        CHECK(s.BatchSize == 100 && s.MaxQueue == 10000 && w.size() == 1);
        s = ParseSettings({{"Url", "http://h:99999"}}, w);
        CHECK(s.Port == 8086);
    }
    std::printf(failures ? "settings: FAILED\n" : "settings: ok\n");
    return failures ? 1 : 0;
}
