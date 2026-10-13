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

std::string ClassName(std::uint32_t classId)
{
    static char const* const names[] = {"", "Warrior", "Paladin", "Hunter", "Rogue", "Priest", "Death Knight", "Shaman", "Mage",
                                        "Warlock", "Monk", "Druid", "Demon Hunter", "Evoker"};
    if (classId >= 1 && classId < sizeof(names) / sizeof(names[0]))
        return names[classId];
    return "class" + std::to_string(classId);
}

std::string LevelBandLabel(std::uint32_t band, std::uint32_t bandCount)
{
    if (bandCount != 0 && band + 1 == bandCount)
        return std::to_string(band * 10) + "+";
    return std::to_string(band * 10) + "-" + std::to_string(band * 10 + 9);
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

Point BuildBotsPoint(StatsSample const& sample, std::int64_t nowNs)
{
    Point point;
    point.Measurement = "bots";
    point.Fields.emplace_back("total", sample.BotsTotal);
    point.Fields.emplace_back("online", sample.BotsOnline);
    point.Fields.emplace_back("queued", sample.BotsQueued);
    point.Fields.emplace_back("logging_in", sample.BotsLoggingIn);
    point.TimestampNs = nowNs;
    return point;
}

std::vector<Point> BuildBotClassPoints(StatsSample const& sample, std::int64_t nowNs)
{
    std::uint32_t last = 12;
    for (std::uint32_t id = 0; id < sample.OnlineBotsByClass.size(); ++id)
        if (sample.OnlineBotsByClass[id] != 0 && id > last)
            last = id;

    std::vector<Point> points;
    for (std::uint32_t id = 1; id <= last; ++id)
    {
        Point point;
        point.Measurement = "bots_by_class";
        point.Tags.emplace_back("class", ClassName(id));
        point.Fields.emplace_back("online", static_cast<std::int64_t>(id < sample.OnlineBotsByClass.size() ? sample.OnlineBotsByClass[id] : 0));
        point.TimestampNs = nowNs;
        points.push_back(std::move(point));
    }
    return points;
}

std::vector<Point> BuildBotLevelPoints(StatsSample const& sample, std::int64_t nowNs)
{
    std::vector<Point> points;
    auto const count = static_cast<std::uint32_t>(sample.OnlineBotsByLevelBand.size());
    for (std::uint32_t band = 0; band < count; ++band)
    {
        Point point;
        point.Measurement = "bots_by_level";
        point.Tags.emplace_back("band", LevelBandLabel(band, count));
        point.Fields.emplace_back("online", static_cast<std::int64_t>(sample.OnlineBotsByLevelBand[band]));
        point.TimestampNs = nowNs;
        points.push_back(std::move(point));
    }
    return points;
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
