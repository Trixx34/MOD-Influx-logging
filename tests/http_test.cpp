// Unit tests for the HTTP transport helpers, and one round trip against a loopback listener. No core needed.
#include "http_transport.h"

#include <boost/asio.hpp>

#include <thread>

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
namespace asio = boost::asio;

// Accepts one connection, reads the request, answers with `status`, and returns what it read.
std::string ServeOnce(asio::io_context& io, asio::ip::tcp::acceptor& acceptor, std::string status)
{
    asio::ip::tcp::socket socket(io);
    acceptor.accept(socket);
    asio::streambuf buf;
    boost::system::error_code ec;
    asio::read_until(socket, buf, "\r\n\r\n", ec);
    std::string request(asio::buffers_begin(buf.data()), asio::buffers_end(buf.data()));
    std::size_t const headEnd = request.find("\r\n\r\n") + 4;
    std::size_t const lengthAt = request.find("Content-Length: ");
    std::size_t const length = std::stoul(request.substr(lengthAt + 16));
    while (request.size() < headEnd + length)
    {
        char chunk[512];
        std::size_t n = socket.read_some(asio::buffer(chunk), ec);
        if (ec) break;
        request.append(chunk, n);
    }
    std::string const reply = "HTTP/1.1 " + status + "\r\nContent-Length: 0\r\n\r\n";
    asio::write(socket, asio::buffer(reply), ec);
    return request;
}

int main()
{
    CHECK(UrlEncode("my org/1") == "my%20org%2F1");
    CHECK(UrlEncode("a-b_c.d~e") == "a-b_c.d~e");

    Settings s; s.Org = "o r"; s.Bucket = "b"; s.Token = "tok"; s.Host = "h"; s.Port = 1;
    std::string const head = BuildWriteRequestHead(s, 42);
    CHECK(head.rfind("POST /api/v2/write?org=o%20r&bucket=b&precision=ns HTTP/1.1\r\n", 0) == 0);
    CHECK(head.find("Authorization: Token tok\r\n") != std::string::npos);
    CHECK(head.find("Content-Length: 42\r\n") != std::string::npos);
    CHECK(head.size() > 4 && head.substr(head.size() - 4) == "\r\n\r\n");

    CHECK(ClassifyStatusLine("HTTP/1.1 204 No Content\r").Ok);
    CHECK(!ClassifyStatusLine("HTTP/1.1 401 Unauthorized").Ok && !ClassifyStatusLine("HTTP/1.1 401 Unauthorized").Retryable);
    CHECK(ClassifyStatusLine("HTTP/1.1 503 x").Retryable && ClassifyStatusLine("HTTP/1.1 429 x").Retryable);
    CHECK(ClassifyStatusLine("garbage").Retryable);

    for (std::string status : {"204 No Content", "401 Unauthorized"})
    {
        asio::io_context io;
        asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
        s.Port = acceptor.local_endpoint().port();
        s.Host = "127.0.0.1"; s.RequestTimeoutMs = 2000;
        std::string seen;
        std::thread server([&] { seen = ServeOnce(io, acceptor, status); });
        SendResult r = MakeHttpTransport(s)("m v=1i");
        server.join();
        CHECK(r.Ok == (status[0] == '2'));
        CHECK(seen.size() > 6 && seen.substr(seen.size() - 6) == "m v=1i");
    }

    // Nothing listening: a retryable network error, not a hang.
    {
        asio::io_context io;
        asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
        s.Port = acceptor.local_endpoint().port();
        acceptor.close();
        SendResult r = MakeHttpTransport(s)("x v=1i");
        CHECK(!r.Ok && r.Retryable);
    }
    std::printf(failures ? "http: FAILED\n" : "http: ok\n");
    return failures ? 1 : 0;
}
