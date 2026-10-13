// influx-stats: writes the core's server and bot statistics to InfluxDB v2 over HTTP.
//
// Usage: influx-stats <tier1-endpoint>. The core passes the endpoint when it spawns the module, and sends it a WorldScript.OnStats
// sample every Modules.Stats.IntervalSec seconds. Everything slow happens in this process, on a worker thread
// (influx_writer.h); the worldserver only queues the sample.

#include "http_transport.h"
#include "influx_settings.h"
#include "influx_writer.h"
#include "server_metrics.h"

#include "forever_module/module_client.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace {

std::int64_t NowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: influx-stats <tier1-endpoint>\n";
        return 1;
    }

    forever_module::ModuleManifest manifest;
    manifest.module_name = "influx-stats";
    manifest.module_version = "0.1.0";
    manifest.tier1_hooks = {"WorldScript.OnStats"};

    forever_module::ModuleClient client(std::move(manifest));
    if (!client.Connect(argv[1]) || !client.Handshake())
    {
        std::cerr << "influx-stats: could not connect to the core: " << client.rejection_reason() << "\n";
        return 1;
    }

    influx_stats::Settings settings;
    std::unique_ptr<influx_stats::Writer> writer;
    std::int64_t const corePid = influx_stats::ParentPid();

    // Config arrives right after the handshake. The writer is built once; a later re-delivery changes nothing (settings are read at start).
    client.OnConfig([&](std::unordered_map<std::string, std::string> const& config)
    {
        if (writer)
            return;
        std::vector<std::string> warnings;
        settings = influx_stats::ParseSettings(config, warnings);
        for (std::string const& warning : warnings)
            std::cerr << "influx-stats: config: " << warning << "\n";
        if (!settings.Enabled)
        {
            client.Log(0, "influx-stats is off (Module.influx-stats.Enabled = 0)");
            return;
        }
        if (std::string const why = influx_stats::ValidateForWriting(settings); !why.empty())
        {
            client.Log(2, "influx-stats stays off: " + why);
            settings.Enabled = false;
            return;
        }
        writer = std::make_unique<influx_stats::Writer>(settings, influx_stats::MakeHttpTransport(settings));
        // The first failure of a run of failures is enough in the core log; the module point carries the counts.
        writer->OnError = [](std::string const& error) { std::cerr << "influx-stats: gave up on a batch: " << error << "\n"; };
        writer->Start();
        client.Log(0, "influx-stats writes to " + settings.Host + ":" + std::to_string(settings.Port) + " bucket " + settings.Bucket);
    });

    client.OnNotify("WorldScript.OnStats", [&](forever_module::EventNotify const& notify)
    {
        if (!writer)
            return;
        forever_module::ServerStatsEvent const& stats = notify.server_stats();
        influx_stats::StatsSample sample;
        sample.PlayersOnline = stats.players_online();
        sample.SessionsQueued = stats.sessions_queued();
        sample.UptimeSeconds = static_cast<std::int64_t>(stats.uptime_seconds());
        sample.UpdateDiffAvgMs = stats.update_diff_avg_ms();
        sample.UpdateDiffMaxMs = stats.update_diff_max_ms();
        sample.UpdateTicks = stats.update_ticks();
        sample.BotsTotal = stats.bots_total();
        sample.BotsOnline = stats.bots_online();
        sample.BotsQueued = stats.bots_queued();
        sample.BotsLoggingIn = stats.bots_logging_in();
        sample.OnlineBotsByClass.assign(stats.online_bots_by_class().begin(), stats.online_bots_by_class().end());
        sample.OnlineBotsByLevelBand.assign(stats.online_bots_by_level_band().begin(), stats.online_bots_by_level_band().end());

        std::int64_t const now = NowNs();
        writer->Enqueue(influx_stats::BuildServerPoint(sample, influx_stats::ReadProcessRssKb(corePid), now));
        writer->Enqueue(influx_stats::BuildBotsPoint(sample, now));
        for (influx_stats::Point& point : influx_stats::BuildBotClassPoints(sample, now))
            writer->Enqueue(std::move(point));
        for (influx_stats::Point& point : influx_stats::BuildBotLevelPoints(sample, now))
            writer->Enqueue(std::move(point));
        writer->Enqueue(influx_stats::BuildModulePoint(writer->Stats(), now));
    });

    client.Run();

    if (writer)
        writer->Stop();
    return 0;
}
