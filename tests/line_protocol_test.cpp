// Unit tests for the line protocol encoder. No core needed.
#include "line_protocol.h"

#include <limits>

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

int main()
{
    using namespace influx_stats;
    Point p;
    p.Measurement = "forever_server";
    p.Tags = {{"realm", "sim 1"}, {"a", "x,y"}};
    p.Fields = {{"players", std::int64_t(5)}, {"load", 0.5}, {"ok", true}, {"note", std::string("say \"hi\"")}};
    p.TimestampNs = 1700000000000000000;
    CHECK(EncodePoint(p) == "forever_server,a=x\\,y,realm=sim\\ 1 players=5i,load=0.5,ok=true,note=\"say \\\"hi\\\"\" 1700000000000000000");

    Point noTime;
    noTime.Measurement = "m";
    noTime.Fields = {{"v", std::int64_t(-3)}};
    CHECK(EncodePoint(noTime) == "m v=-3i");

    Point noFields;
    noFields.Measurement = "m";
    CHECK(EncodePoint(noFields).empty());

    Point nan;
    nan.Measurement = "m";
    nan.Fields = {{"bad", std::numeric_limits<double>::quiet_NaN()}, {"good", std::int64_t(1)}};
    CHECK(EncodePoint(nan) == "m good=1i");

    Point onlyNan;
    onlyNan.Measurement = "m";
    onlyNan.Fields = {{"bad", std::numeric_limits<double>::infinity()}};
    CHECK(EncodePoint(onlyNan).empty());

    Point emptyTag;
    emptyTag.Measurement = "m";
    emptyTag.Tags = {{"k", ""}, {"", "v"}};
    emptyTag.Fields = {{"v", std::int64_t(1)}};
    CHECK(EncodePoint(emptyTag) == "m v=1i");

    Point newline;
    newline.Measurement = "m";
    newline.Fields = {{"s", std::string("a\nb")}};
    CHECK(EncodePoint(newline) == "m s=\"a b\"");

    CHECK(EncodeBatch({noTime, noFields, noTime}) == "m v=-3i\nm v=-3i");
    CHECK(EncodeBatch({}).empty());

    std::printf(failures ? "line_protocol: FAILED\n" : "line_protocol: ok\n");
    return failures ? 1 : 0;
}
