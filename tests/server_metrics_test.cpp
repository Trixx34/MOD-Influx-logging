// Unit tests for the server metrics helpers. No core needed.
#include "server_metrics.h"

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
    CHECK(ParseVmRssKb("Name:\tworldserver\nVmRSS:\t  123456 kB\nThreads:\t4\n") == 123456);
    CHECK(ParseVmRssKb("Name:\tx\n") == -1);
    CHECK(ParseVmRssKb("VmRSS:\tbad\n") == -1);
    CHECK(ReadProcessRssKb(-1) == -1);
#ifdef __linux__
    CHECK(ReadProcessRssKb(ParentPid()) > 0 || ParentPid() <= 0);
#endif

    StatsSample sample;
    sample.PlayersOnline = 3; sample.SessionsQueued = 1; sample.UptimeSeconds = 60; sample.UpdateDiffAvgMs = 20;
    sample.UpdateDiffMaxMs = 90; sample.UpdateTicks = 300; sample.BotsTotal = 10; sample.BotsOnline = 8; sample.BotsQueued = 1;
    sample.BotsLoggingIn = 1;
    sample.OnlineBotsByClass = {0, 5, 0, 3};
    sample.OnlineBotsByLevelBand = {2, 0, 6};

    CHECK(EncodePoint(BuildServerPoint(sample, 1000, 5)) ==
          "server players_online=3i,sessions_queued=1i,uptime_s=60i,update_diff_avg_ms=20i,update_diff_max_ms=90i,update_ticks=300i,rss_kb=1000i 5");
    CHECK(EncodePoint(BuildServerPoint(sample, -1, 5)).find("rss_kb") == std::string::npos);
    CHECK(EncodePoint(BuildBotsPoint(sample, 5)) == "bots total=10i,online=8i,queued=1i,logging_in=1i 5");

    CHECK(ClassName(1) == "Warrior" && ClassName(12) == "Demon Hunter" && ClassName(40) == "class40");
    CHECK(LevelBandLabel(4, 12) == "40-49" && LevelBandLabel(11, 12) == "110+");

    auto classes = BuildBotClassPoints(sample, 5);
    CHECK(classes.size() == 12);
    CHECK(EncodePoint(classes[0]) == "bots_by_class,class=Warrior online=5i 5");
    CHECK(EncodePoint(classes[1]) == "bots_by_class,class=Paladin online=0i 5");
    CHECK(EncodePoint(classes[2]) == "bots_by_class,class=Hunter online=3i 5");
    sample.OnlineBotsByClass.assign(15, 0);
    sample.OnlineBotsByClass[14] = 2;
    CHECK(BuildBotClassPoints(sample, 5).size() == 14);

    sample.OnlineBotsByLevelBand = {2, 0, 6};
    auto levels = BuildBotLevelPoints(sample, 5);
    CHECK(levels.size() == 3);
    CHECK(EncodePoint(levels[0]) == "bots_by_level,band=0-9 online=2i 5");
    CHECK(EncodePoint(levels[2]) == "bots_by_level,band=20+ online=6i 5");

    WriterStats s;
    s.Queued = 1; s.Written = 2; s.Dropped = 3; s.Failures = 4;
    CHECK(EncodePoint(BuildModulePoint(s, 7)) == "module queued=1i,written=2i,dropped=3i,failures=4i 7");

    std::printf(failures ? "server_metrics: FAILED\n" : "server_metrics: ok\n");
    return failures ? 1 : 0;
}
