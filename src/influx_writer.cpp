#include "influx_writer.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace influx_stats {

Writer::Writer(Settings settings, Transport transport, std::chrono::milliseconds retryBackoff)
    : _settings(std::move(settings)), _transport(std::move(transport)), _retryBackoff(retryBackoff)
{
}

Writer::~Writer()
{
    Stop();
}

void Writer::Start()
{
    std::lock_guard lock(_mutex);
    if (_worker.joinable())
        return;
    _stopping = false;
    _worker = std::thread([this] { Run(); });
}

void Writer::Stop()
{
    {
        std::lock_guard lock(_mutex);
        if (!_worker.joinable())
            return;
        _stopping = true;
    }
    _wake.notify_all();
    _worker.join();
}

void Writer::Enqueue(Point point)
{
    point.Measurement = _settings.MeasurementPrefix + point.Measurement;
    for (auto const& [key, value] : _settings.GlobalTags)
    {
        bool const present = std::any_of(point.Tags.begin(), point.Tags.end(), [&key](auto const& tag) { return tag.first == key; });
        if (!present)
            point.Tags.emplace_back(key, value);
    }

    bool full = false;
    {
        std::lock_guard lock(_mutex);
        if (_queue.size() >= _settings.MaxQueue)
        {
            _queue.pop_front();
            _dropped.fetch_add(1);
        }
        _queue.push_back(std::move(point));
        full = _queue.size() >= _settings.BatchSize;
    }
    if (full)
        _wake.notify_one();
}

WriterStats Writer::Stats() const
{
    WriterStats stats;
    {
        std::lock_guard lock(_mutex);
        stats.Queued = _queue.size();
    }
    stats.Written = _written.load();
    stats.Dropped = _dropped.load();
    stats.Failures = _failures.load();
    return stats;
}

bool Writer::SendWithRetry(std::string const& body, std::size_t pointCount)
{
    std::chrono::milliseconds backoff = _retryBackoff;
    std::string lastError;
    for (int attempt = 1; attempt <= MaxAttempts; ++attempt)
    {
        SendResult const result = _transport(body);
        if (result.Ok)
        {
            _written.fetch_add(pointCount);
            return true;
        }
        _failures.fetch_add(1);
        lastError = result.Error;
        if (!result.Retryable || attempt == MaxAttempts)
            break;

        // Wait out the backoff, but leave at once when stopping: the last flush gets one attempt only.
        std::unique_lock lock(_mutex);
        if (_wake.wait_for(lock, backoff, [this] { return _stopping; }))
            break;
        backoff *= 2;
    }
    _dropped.fetch_add(pointCount);
    if (OnError)
        OnError(lastError);
    return false;
}

void Writer::Run()
{
    for (;;)
    {
        std::vector<Point> batch;
        bool stopping = false;
        {
            std::unique_lock lock(_mutex);
            _wake.wait_for(lock, std::chrono::milliseconds(_settings.FlushIntervalMs),
                           [this] { return _stopping || _queue.size() >= _settings.BatchSize; });
            stopping = _stopping;
            std::size_t const count = std::min<std::size_t>(_queue.size(), _settings.BatchSize);
            batch.assign(std::make_move_iterator(_queue.begin()), std::make_move_iterator(_queue.begin() + count));
            _queue.erase(_queue.begin(), _queue.begin() + count);
        }

        if (!batch.empty())
        {
            std::string const body = EncodeBatch(batch);
            if (!body.empty())
                SendWithRetry(body, batch.size());
        }

        if (stopping)
        {
            // Shutdown flushes one batch; the rest is dropped so a dead InfluxDB cannot hold the module up.
            std::lock_guard lock(_mutex);
            _dropped.fetch_add(_queue.size());
            _queue.clear();
            return;
        }
    }
}

} // namespace influx_stats
