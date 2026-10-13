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
};

// measurement "server": players_online, sessions_queued, uptime_s, update_diff_avg_ms, update_diff_max_ms, update_ticks (all
// int), and rss_kb (int, left out when unknown).
Point BuildServerPoint(StatsSample const& sample, std::int64_t rssKb, std::int64_t nowNs);

// measurement "module": the module's own health -- queued, written, dropped, failures (all int).
Point BuildModulePoint(WriterStats const& stats, std::int64_t nowNs);

} // namespace influx_stats
