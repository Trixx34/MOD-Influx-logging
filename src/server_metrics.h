// Turns the core's WorldScript.OnStats sample, and what only this process can read (the worldserver's memory, from /proc on
// Linux), into points. No SDK or core needed (tests/server_metrics_test.cpp).
#pragma once

#include "influx_writer.h"
#include "line_protocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace influx_stats {

// The VmRSS line of /proc/<pid>/status, in kB. -1 when the text has none.
std::int64_t ParseVmRssKb(std::string const& procStatus);

// Resident memory of a process in kB, or -1 where it cannot be read (not Linux, or the process is gone).
std::int64_t ReadProcessRssKb(std::int64_t pid);

// The pid of the process that started this module (the worldserver), or -1 where unknown.
std::int64_t ParentPid();

// The core's sample (ServerStatsEvent in the SDK's events.proto), copied out so this code does not depend on the SDK.
struct StatsSample
{
    std::int64_t PlayersOnline = 0;
    std::int64_t SessionsQueued = 0;
    std::int64_t UptimeSeconds = 0;
    std::int64_t UpdateDiffAvgMs = 0;
    std::int64_t UpdateDiffMaxMs = 0;
    std::int64_t UpdateTicks = 0;
    std::int64_t BotsTotal = 0;
    std::int64_t BotsOnline = 0;
    std::int64_t BotsQueued = 0;
    std::int64_t BotsLoggingIn = 0;
    std::vector<std::uint32_t> OnlineBotsByClass;     // index = class id
    std::vector<std::uint32_t> OnlineBotsByLevelBand; // index i = levels 10*i to 10*i+9 (the last band is open-ended)
};

// "Warrior" for class id 1 and so on; "class<N>" for an id this module does not know.
std::string ClassName(std::uint32_t classId);

// "40-49" for band 4; the last band of `bandCount` bands reads "110+" style ("110+").
std::string LevelBandLabel(std::uint32_t band, std::uint32_t bandCount);

// measurement "server": players_online, sessions_queued, uptime_s, update_diff_avg_ms, update_diff_max_ms, update_ticks (all
// int), and rss_kb (int, left out when unknown).
Point BuildServerPoint(StatsSample const& sample, std::int64_t rssKb, std::int64_t nowNs);

// measurement "bots": total, online, queued, logging_in (all int).
Point BuildBotsPoint(StatsSample const& sample, std::int64_t nowNs);

// measurement "bots_by_class", tag class=<name>, field online (int). One point per class id from 1 up to the last id with a count
// or id 12, whichever is higher, so a class that dropped to zero shows as zero rather than as a gap.
std::vector<Point> BuildBotClassPoints(StatsSample const& sample, std::int64_t nowNs);

// measurement "bots_by_level", tag band=<label>, field online (int). One point per band.
std::vector<Point> BuildBotLevelPoints(StatsSample const& sample, std::int64_t nowNs);

// measurement "module": the module's own health -- queued, written, dropped, failures (all int).
Point BuildModulePoint(WriterStats const& stats, std::int64_t nowNs);

} // namespace influx_stats
