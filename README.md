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
| `forever_module` | | `queued`, `written`, `dropped`, `failures` (the writer's own counters) |

Bot figures (counts, deaths, quests, stuck episodes) are not here: the bots module (MOD-Forever-Bots) reports them itself, using this
repository's writer as a library (see below).

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

## Using the writer from another module

The CMake file defines a library target `influx_logging` (line protocol encoder, settings parser, batching writer, HTTP transport). When this
repository is added as a subdirectory (`FetchContent` or `add_subdirectory`), only the library is built, not the module or its tests.
MOD-Forever-Bots uses it for its bot statistics.
