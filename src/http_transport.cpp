#include "http_transport.h"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdio>
#include <istream>

namespace influx_stats {

namespace asio = boost::asio;
using asio::ip::tcp;

std::string UrlEncode(std::string const& text)
{
    std::string out;
    for (unsigned char c : text)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            out += static_cast<char>(c);
        else
        {
            char buffer[4];
            std::snprintf(buffer, sizeof(buffer), "%%%02X", c);
            out += buffer;
        }
    }
    return out;
}

std::string BuildWriteRequestHead(Settings const& settings, std::size_t bodySize)
{
    std::string head = "POST /api/v2/write?org=" + UrlEncode(settings.Org) + "&bucket=" + UrlEncode(settings.Bucket) +
                       "&precision=ns HTTP/1.1\r\n";
    head += "Host: " + settings.Host + ":" + std::to_string(settings.Port) + "\r\n";
    head += "Authorization: Token " + settings.Token + "\r\n";
    head += "Content-Type: text/plain; charset=utf-8\r\n";
    head += "Content-Length: " + std::to_string(bodySize) + "\r\n";
    head += "Connection: close\r\n\r\n";
    return head;
}

SendResult ClassifyStatusLine(std::string const& statusLine)
{
    SendResult result;
    int code = 0;
    if (std::sscanf(statusLine.c_str(), "HTTP/%*d.%*d %d", &code) != 1)
    {
        result.Retryable = true;
        result.Error = "unreadable HTTP status line";
        return result;
    }
    if (code >= 200 && code < 300)
    {
        result.Ok = true;
        return result;
    }
    result.Retryable = code == 429 || code >= 500;
    result.Error = "InfluxDB answered HTTP " + std::to_string(code);
    if (code == 401 || code == 403)
        result.Error += " (check Module.influx-stats.Token)";
    else if (code == 404)
        result.Error += " (check Module.influx-stats.Org and Bucket)";
    return result;
}

namespace {

SendResult NetworkError(std::string const& what)
{
    SendResult result;
    result.Retryable = true;
    result.Error = what;
    return result;
}

SendResult Post(Settings const& settings, std::string const& body)
{
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(settings.RequestTimeoutMs);
    asio::io_context io;
    boost::system::error_code error;

    // Runs the io_context until `done` or the shared deadline.
    auto const wait = [&](bool& done) {
        io.restart();
        while (!done && !io.stopped())
        {
            auto const left = deadline - std::chrono::steady_clock::now();
            if (left <= std::chrono::steady_clock::duration::zero())
                return false;
            io.run_one_for(left);
        }
        return done;
    };

    tcp::resolver resolver(io);
    auto const endpoints = resolver.resolve(settings.Host, std::to_string(settings.Port), error);
    if (error)
        return NetworkError("cannot resolve " + settings.Host + ": " + error.message());

    tcp::socket socket(io);
    bool done = false;
    asio::async_connect(socket, endpoints, [&](boost::system::error_code const& ec, tcp::endpoint const&) { error = ec; done = true; });
    if (!wait(done))
        return NetworkError("connect to " + settings.Host + " timed out");
    if (error)
        return NetworkError("cannot connect to " + settings.Host + ":" + std::to_string(settings.Port) + ": " + error.message());

    std::string const request = BuildWriteRequestHead(settings, body.size()) + body;
    done = false;
    asio::async_write(socket, asio::buffer(request), [&](boost::system::error_code const& ec, std::size_t) { error = ec; done = true; });
    if (!wait(done))
        return NetworkError("write to InfluxDB timed out");
    if (error)
        return NetworkError("write to InfluxDB failed: " + error.message());

    asio::streambuf response;
    done = false;
    asio::async_read_until(socket, response, "\r\n", [&](boost::system::error_code const& ec, std::size_t) { error = ec; done = true; });
    if (!wait(done))
        return NetworkError("InfluxDB did not answer in time");
    if (error)
        return NetworkError("reading the InfluxDB answer failed: " + error.message());

    std::istream stream(&response);
    std::string statusLine;
    std::getline(stream, statusLine);
    return ClassifyStatusLine(statusLine);
}

} // namespace

Transport MakeHttpTransport(Settings settings)
{
    return [settings = std::move(settings)](std::string const& body) { return Post(settings, body); };
}

} // namespace influx_stats
