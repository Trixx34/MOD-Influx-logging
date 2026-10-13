# MOD-Influx-logging (module `influx-stats`)

Writes server and bot statistics to InfluxDB v2. Off by default.

The core samples itself every `Modules.Stats.IntervalSec` seconds (worldserver.conf, 0 = off) and sends the sample to modules that
list the hook `WorldScript.OnStats`. This module turns each sample into line protocol and writes it from its own process, on a worker
thread, so the world tick never waits on the network. If InfluxDB is slow or down the queue fills to `MaxQueue`, the oldest points
are dropped and counted, and nothing else happens to the server.

## Measurements (prefix `forever_` by default)

| Measurement | Tags | Fields |
|---|---|---|
| `forever_server` | | `players_online` (not bots), `sessions_queued`, `uptime_s`, `update_diff_avg_ms`, `update_diff_max_ms`, `update_ticks`, `rss_kb` (worldserver memory; Linux only) |
| `forever_bots` | | `total`, `online`, `queued`, `logging_in` |
| `forever_bots_by_class` | `class` | `online` |
| `forever_bots_by_level` | `band` (`0-9`, `10-19`, ...) | `online` |
| `forever_module` | | `queued`, `written`, `dropped`, `failures` (the writer's own counters) |

Not yet: bot deaths, quests done, auction house activity and stuck/loop events. Those are events inside the bot code, not state the
core can sample; each needs a counter at its source and a field in the sample.

## Needs

- A core with the `WorldScript.OnStats` hook (`ModuleMgr::OnStatsTick`, Trinity-Forever-Modules branch `feature/influx-stats-module`) and an SDK with `ServerStatsEvent`
  (forever-module-sdk branch `feature/server-stats-event`). An older core refuses the module at handshake.
- `Modules.Stats.IntervalSec` above 0 in worldserver.conf.
- Plain `http://` to InfluxDB (local, or a reverse proxy).

## Settings

See `config/modules.d/influx-stats.conf.example` (every entry is annotated). Copy it to `worldserver.conf.d/modules.d/influx-stats.conf`,
set `Enabled`, `Org`, `Bucket` and `Token`.

## Build and test

Clone into `modules/influx-stats/` of the core checkout (the directory name must match `module.json`); the core builds it like any module. Stand-alone:

    cmake -S . -B build -DFOREVER_MODULE_SDK_SOURCE=/path/to/forever-module-sdk
    cmake --build build && ctest --test-dir build
