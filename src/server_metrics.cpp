#include "server_metrics.h"

#include <fstream>
#include <sstream>

#ifdef __linux__
#include <unistd.h>
#endif

namespace influx_stats {

std::int64_t ParseVmRssKb(std::string const& procStatus)
{
    std::istringstream stream(procStatus);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.rfind("VmRSS:", 0) != 0)
            continue;
        try
        {
            return std::stoll(line.substr(6));
        }
        catch (...)
        {
            return -1;
        }
    }
    return -1;
}

std::int64_t ReadProcessRssKb(std::int64_t pid)
{
#ifdef __linux__
    if (pid <= 0)
        return -1;
    std::ifstream file("/proc/" + std::to_string(pid) + "/status");
    if (!file)
        return -1;
    std::stringstream content;
    content << file.rdbuf();
    return ParseVmRssKb(content.str());
#else
    (void) pid;
    return -1;
#endif
}

std::int64_t ParentPid()
{
#ifdef __linux__
    return getppid();
#else
    return -1;
#endif
}

Point BuildServerPoint(StatsSample const& sample, std::int64_t rssKb, std::int64_t nowNs)
{
    Point point;
    point.Measurement = "server";
    point.Fields.emplace_back("players_online", sample.PlayersOnline);
    point.Fields.emplace_back("sessions_queued", sample.SessionsQueued);
    point.Fields.emplace_back("uptime_s", sample.UptimeSeconds);
    point.Fields.emplace_back("update_diff_avg_ms", sample.UpdateDiffAvgMs);
    point.Fields.emplace_back("update_diff_max_ms", sample.UpdateDiffMaxMs);
    point.Fields.emplace_back("update_ticks", sample.UpdateTicks);
    if (rssKb >= 0)
        point.Fields.emplace_back("rss_kb", rssKb);
    point.TimestampNs = nowNs;
    return point;
}

Point BuildModulePoint(WriterStats const& stats, std::int64_t nowNs)
{
    Point point;
    point.Measurement = "module";
    point.Fields.emplace_back("queued", static_cast<std::int64_t>(stats.Queued));
    point.Fields.emplace_back("written", static_cast<std::int64_t>(stats.Written));
    point.Fields.emplace_back("dropped", static_cast<std::int64_t>(stats.Dropped));
    point.Fields.emplace_back("failures", static_cast<std::int64_t>(stats.Failures));
    point.TimestampNs = nowNs;
    return point;
}

} // namespace influx_stats
