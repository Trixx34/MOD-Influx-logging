// Batching, asynchronous writer. Enqueue() only appends to a bounded in-memory queue; a worker thread encodes batches and hands
// them to a Transport, so no network call ever happens on the caller's thread. When the queue is full the oldest points are
// dropped (and counted), so an unreachable InfluxDB costs memory up to MaxQueue and nothing more.
#pragma once

#include "influx_settings.h"
#include "line_protocol.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace influx_stats {

struct SendResult
{
    bool Ok = false;
    bool Retryable = false; // a network error or a 5xx/429; a 4xx (bad token, bad bucket) is not retried
    std::string Error;
};

// Sends one request body (line protocol). Called only from the writer's worker thread.
using Transport = std::function<SendResult(std::string const& body)>;

struct WriterStats
{
    std::uint64_t Queued = 0;   // points waiting now
    std::uint64_t Written = 0;  // points InfluxDB accepted
    std::uint64_t Dropped = 0;  // points lost to a full queue or a failed batch
    std::uint64_t Failures = 0; // failed send attempts
};

class Writer
{
public:
    // retryBackoff is the first wait between attempts; it doubles each time. A batch is attempted MaxAttempts times.
    Writer(Settings settings, Transport transport, std::chrono::milliseconds retryBackoff = std::chrono::milliseconds(1000));
    ~Writer();

    Writer(Writer const&) = delete;
    Writer& operator=(Writer const&) = delete;

    void Start();
    // Stops the worker after one last attempt to flush what is queued. Safe to call twice.
    void Stop();

    // Adds the measurement prefix and global tags, then queues. Never blocks on the network.
    void Enqueue(Point point);

    WriterStats Stats() const;

    // Called by the worker with the last error each time a batch is given up on. Set before Start().
    std::function<void(std::string const&)> OnError;

    static constexpr int MaxAttempts = 3;

private:
    void Run();
    bool SendWithRetry(std::string const& body, std::size_t pointCount);

    Settings _settings;
    Transport _transport;
    std::chrono::milliseconds _retryBackoff;

    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<Point> _queue;
    bool _stopping = false;
    std::thread _worker;

    std::atomic<std::uint64_t> _written{0};
    std::atomic<std::uint64_t> _dropped{0};
    std::atomic<std::uint64_t> _failures{0};
};

} // namespace influx_stats
