// Unit tests for the batching writer with a fake transport. No core or network needed.
#include "influx_writer.h"

#include <atomic>
#include <thread>
#include <vector>

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

using namespace influx_stats;

Point P(int v)
{
    Point p;
    p.Measurement = "m";
    p.Fields = {{"v", std::int64_t(v)}};
    return p;
}

template <class F> bool WaitFor(F f)
{
    for (int i = 0; i < 400; ++i)
    {
        if (f())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

int main()
{
    // Prefix and global tags are applied; a full batch is sent without waiting for the interval.
    {
        Settings s;
        s.BatchSize = 2; s.FlushIntervalMs = 60000; s.MeasurementPrefix = "forever_"; s.GlobalTags = {{"realm", "r"}};
        std::mutex m; std::vector<std::string> bodies;
        Writer w(s, [&](std::string const& b) { std::lock_guard l(m); bodies.push_back(b); return SendResult{true, false, ""}; });
        w.Start();
        w.Enqueue(P(1)); w.Enqueue(P(2));
        CHECK(WaitFor([&] { std::lock_guard l(m); return bodies.size() == 1; }));
        { std::lock_guard l(m); CHECK(bodies.size() == 1 && bodies[0] == "forever_m,realm=r v=1i\nforever_m,realm=r v=2i"); }
        CHECK(WaitFor([&] { return w.Stats().Written == 2; }));
        w.Stop();
    }
    // A partial batch goes out when the interval passes.
    {
        Settings s; s.BatchSize = 100; s.FlushIntervalMs = 100; s.MeasurementPrefix = "";
        std::atomic<int> calls{0};
        Writer w(s, [&](std::string const&) { ++calls; return SendResult{true, false, ""}; });
        w.Start();
        w.Enqueue(P(1));
        CHECK(WaitFor([&] { return calls == 1; }));
        w.Stop();
    }
    // The queue is bounded: the oldest points are dropped and counted. The worker is not started, so nothing drains.
    {
        Settings s; s.BatchSize = 2; s.MaxQueue = 3;
        Writer w(s, [](std::string const&) { return SendResult{true, false, ""}; });
        for (int i = 0; i < 5; ++i) w.Enqueue(P(i));
        CHECK(w.Stats().Queued == 3 && w.Stats().Dropped == 2);
    }
    // A retryable failure is tried MaxAttempts times, then the batch is dropped and reported.
    {
        Settings s; s.BatchSize = 1; s.FlushIntervalMs = 50; s.MeasurementPrefix = "";
        std::atomic<int> calls{0}; std::atomic<int> errors{0};
        Writer w(s, [&](std::string const&) { ++calls; return SendResult{false, true, "down"}; }, std::chrono::milliseconds(1));
        w.OnError = [&](std::string const& e) { if (e == "down") ++errors; };
        w.Start();
        w.Enqueue(P(1));
        CHECK(WaitFor([&] { return w.Stats().Dropped == 1; }));
        CHECK(calls == Writer::MaxAttempts && errors == 1 && w.Stats().Failures == 3 && w.Stats().Written == 0);
        w.Stop();
    }
    // A non-retryable failure (bad token) is tried once.
    {
        Settings s; s.BatchSize = 1; s.FlushIntervalMs = 50; s.MeasurementPrefix = "";
        std::atomic<int> calls{0};
        Writer w(s, [&](std::string const&) { ++calls; return SendResult{false, false, "401"}; }, std::chrono::milliseconds(1));
        w.Start();
        w.Enqueue(P(1));
        CHECK(WaitFor([&] { return w.Stats().Dropped == 1; }));
        CHECK(calls == 1);
        w.Stop();
    }
    // Stop flushes what is queued.
    {
        Settings s; s.BatchSize = 100; s.FlushIntervalMs = 60000; s.MeasurementPrefix = "";
        std::atomic<int> calls{0};
        Writer w(s, [&](std::string const&) { ++calls; return SendResult{true, false, ""}; });
        w.Start();
        w.Enqueue(P(1));
        w.Stop();
        CHECK(calls == 1 && w.Stats().Written == 1);
        w.Stop();
    }
    std::printf(failures ? "writer: FAILED\n" : "writer: ok\n");
    return failures ? 1 : 0;
}
