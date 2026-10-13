// The Transport that posts line protocol to InfluxDB v2 (POST /api/v2/write) over plain HTTP.
#pragma once

#include "influx_settings.h"
#include "influx_writer.h"

#include <string>

namespace influx_stats {

// Percent-encodes everything but unreserved characters (RFC 3986), for the org and bucket query values.
std::string UrlEncode(std::string const& text);

// The request line and headers up to and including the blank line; the body follows. Content-Length is body.size().
std::string BuildWriteRequestHead(Settings const& settings, std::size_t bodySize);

// Maps a status line ("HTTP/1.1 204 No Content") to a result. 2xx is ok; 429 and 5xx are retryable; anything else is a fault in
// the request or credentials that retrying will not fix.
SendResult ClassifyStatusLine(std::string const& statusLine);

// One connection per request, bounded by Settings.RequestTimeoutMs for the whole exchange.
Transport MakeHttpTransport(Settings settings);

} // namespace influx_stats
