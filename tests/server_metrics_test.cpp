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
    sample.UpdateDiffMaxMs = 90; sample.UpdateTicks = 300;

    CHECK(EncodePoint(BuildServerPoint(sample, 1000, 5)) ==
          "server players_online=3i,sessions_queued=1i,uptime_s=60i,update_diff_avg_ms=20i,update_diff_max_ms=90i,update_ticks=300i,rss_kb=1000i 5");
    CHECK(EncodePoint(BuildServerPoint(sample, -1, 5)).find("rss_kb") == std::string::npos);

    WriterStats s;
    s.Queued = 1; s.Written = 2; s.Dropped = 3; s.Failures = 4;
    CHECK(EncodePoint(BuildModulePoint(s, 7)) == "module queued=1i,written=2i,dropped=3i,failures=4i 7");

    std::printf(failures ? "server_metrics: FAILED\n" : "server_metrics: ok\n");
    return failures ? 1 : 0;
}
