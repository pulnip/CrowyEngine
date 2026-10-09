#include <chrono>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "CommandPort.hpp"
#include "JsonLoader.hpp"
#include "Object.hpp"
#include "Socket.hpp"

using namespace Crowy;

// what the reflection verbs see: a parent chain, ranged and documented
// leaves, an enum with a negative enumerator, one nested struct
struct PortNested{
    f32 amount = 0.25f;
};

CROWY_STRUCT(PortNested)
    .SetProperty("amount", &PortNested::amount)
CROWY_STRUCT_END(PortNested)

struct PortProbeBase{
    f32 exposure = 1.0f;
};

CROWY_STRUCT(PortProbeBase)
    .SetProperty("exposure", &PortProbeBase::exposure)
CROWY_STRUCT_END(PortProbeBase)

enum class PortBlend: i16{
    Opaque = -1,
    Masked = 7,
    Additive = 300
};

namespace Crowy
{
    CROWY_ENUM_BEGIN(PortBlend)
        CROWY_ENUM_VALUE(Opaque)
        CROWY_ENUM_VALUE(Masked)
        CROWY_ENUM_VALUE(Additive)
    CROWY_ENUM_END()
}

struct PortProbe: PortProbeBase{
    f32 roughness = 0.5f;
    Vec3 tint{1.0f, 0.5f, 0.25f};
    PortBlend blend = PortBlend::Masked;
    PortNested nested;
};

CROWY_STRUCT(PortProbe)
    .Inherits<PortProbeBase>()
    .SetProperty("roughness", &PortProbe::roughness)
        .SetUIRange(0.0f, 1.0f)
        .SetTooltip("microfacet spread")
    .SetProperty("tint", &PortProbe::tint)
    .SetProperty("blend", &PortProbe::blend)
    .SetProperty("nested", &PortProbe::nested)
CROWY_STRUCT_END(PortProbe)

namespace
{
    // short enough that the timeout cases finish inside a test, long enough
    // that a slow CI box does not trip them by accident
    constexpr f64 TestTimeoutSeconds = 0.05;
    constexpr auto PastTimeout = std::chrono::milliseconds(80);

    struct Response {
        int code = 0;
        Str body;
        DOM::Value json;

        bool ok() const { return json.get<bool>("ok").value_or(false); }
        Str error() const { return json.get<Str>("error").value_or(""); }
    };

    Response parseResponse(const Str& raw) {
        Response response;

        const auto lineEnd = raw.find("\r\n");
        const auto headEnd = raw.find("\r\n\r\n");
        EXPECT_TRUE(lineEnd != Str::npos && headEnd != Str::npos) << raw;
        if(lineEnd == Str::npos || headEnd == Str::npos)
            return response;

        const auto statusLine = StrView(raw).substr(0, lineEnd);
        EXPECT_TRUE(statusLine.starts_with("HTTP/1.1 ")) << statusLine;
        response.code = std::stoi(Str(statusLine.substr(9, 3)));

        const auto head = StrView(raw).substr(0, headEnd);
        response.body = raw.substr(headEnd + 4);

        EXPECT_TRUE(head.find("Connection: close") != StrView::npos) << head;
        const auto expectedLength =
            std::format("Content-Length: {}", response.body.size());
        EXPECT_TRUE(head.find(expectedLength) != StrView::npos) << head;

        response.json = parseJsonString(response.body);

        return response;
    }

    Str post(StrView body) {
        return std::format(
            "POST /rpc HTTP/1.1\r\nContent-Length: {}\r\n\r\n{}",
            body.size(),
            body
        );
    }

    // loopback delivery is asynchronous, so anything the other side
    // "should have seen by now" is pumped for instead of asserted at once
    template<typename Pred>
    bool pump(CommandPort& port, Pred&& done, int maxDrains = 200) {
        for(int i = 0; i < maxDrains; ++i) {
            port.Drain();
            if(done())
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return false;
    }

    // a blocking connect, then non-blocking so the same thread can pump
    // the port between reads
    class Client {
    private:
        Socket::Handle socket = Socket::Invalid;

    public:
        ~Client() {
            if(socket != Socket::Invalid)
                Socket::closeSocket(socket);
        }
        CROWY_DECLARE_PINNED(Client)

        explicit Client(u16 port)
            : socket(Socket::createTcp()) {
            Socket::initialize();
            EXPECT_TRUE(Socket::connectLoopback(socket, port));
            EXPECT_TRUE(Socket::setNonBlocking(socket));
        }

        void Send(StrView text) {
            usize sent = 0;
            while(sent < text.size()) {
                const auto n = Socket::sendSome(
                    socket,
                    text.data() + sent,
                    text.size() - sent
                );
                ASSERT_GT(n, 0);
                sent += static_cast<usize>(n);
            }
        }

        // drains the port until the server closes the connection
        Str Receive(CommandPort& port, int maxDrains = 200) {
            Str received;
            char buffer[4096];

            for(int i = 0; i < maxDrains; ++i) {
                port.Drain();

                while(true) {
                    const auto n =
                        Socket::receiveSome(socket, buffer, sizeof(buffer));
                    if(n > 0) {
                        received.append(buffer, static_cast<usize>(n));

                        continue;
                    }
                    if(n == 0)
                        return received;
                    EXPECT_TRUE(Socket::wouldBlock());

                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            ADD_FAILURE() << "server never closed the connection; got: "
                          << received;

            return received;
        }

        bool Closed() {
            char byte;
            const auto n = Socket::receiveSome(socket, &byte, 1);

            return n == 0;
        }
    };
}

class CommandPortTest: public ::testing::Test {
protected:
    CommandPort port{CommandPortConfig{
        .port = 0,
        .pendingTimeoutSeconds = TestTimeoutSeconds,
        .idleTimeoutSeconds = TestTimeoutSeconds
    }};

    void SetUp() override {
        ASSERT_NE(port.Port(), 0);
        ASSERT_EQ(port.Status().server, CommandPortStatus::Server::Listening);

        port.RegisterVerb("ping", [](const DOM::Value&, Reply reply) {
            DOM::Table result;
            result.emplace("pong", DOM::Value(true));
            reply.Ok(DOM::Value(std::move(result)));
        });
    }

    Response exchange(StrView request) {
        Client client(port.Port());
        client.Send(request);

        return parseResponse(client.Receive(port));
    }
};

TEST_F(CommandPortTest, PingRoundTrip) {
    Client client(port.Port());
    client.Send(post(R"({"cmd":"ping"})"));

    // how many drains the client needs to see the close depends on the
    // loopback stack, so the drain is read where the verb is dispatched
    ASSERT_TRUE(pump(port, [&] { return port.Status().lastVerb == "ping"; }));
    EXPECT_EQ(port.Status().lastVerbDrain, port.Status().drainCount);

    const auto response = parseResponse(client.Receive(port));

    EXPECT_EQ(response.code, 200);
    EXPECT_TRUE(response.ok()) << response.body;
    EXPECT_EQ(response.json.get<bool>("result.pong"), true);

    const auto status = port.Status();
    EXPECT_TRUE(status.everConnected);
    EXPECT_FALSE(status.lastReplyFailed);
}

TEST_F(CommandPortTest, InfoListsRegisteredVerbs) {
    port.RegisterVerb("alpha", [](const DOM::Value&, Reply reply) {
        reply.Ok(DOM::Value(true));
    });

    const auto response = exchange("GET / HTTP/1.1\r\n\r\n");

    EXPECT_EQ(response.code, 200);
    ASSERT_TRUE(response.ok()) << response.body;
    std::vector<Str> verbs;
    response.json.forEach("result.verbs", [&](const DOM::Value& v) {
        verbs.push_back(*v.asString());
    });
    EXPECT_EQ(verbs, (std::vector<Str>{
        "alpha", "describe", "get_property", "list_objects", "ping", "set_property"
    }));
}

TEST_F(CommandPortTest, HeaderNamesAreCaseInsensitiveAndContentTypeIgnored) {
    const auto response = exchange(
        "POST /rpc HTTP/1.0\r\n"
        "content-length: 14\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "\r\n"
        R"({"cmd":"ping"})"
    );

    EXPECT_EQ(response.code, 200);
    EXPECT_TRUE(response.ok()) << response.body;
}

TEST_F(CommandPortTest, UnknownVerbIsAnError) {
    const auto response = exchange(post(R"({"cmd":"nope"})"));

    EXPECT_EQ(response.code, 200);
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.error(), "unknown verb 'nope'");
}

TEST_F(CommandPortTest, MissingCmdIsAnError) {
    const auto response = exchange(post(R"({"args":{}})"));

    EXPECT_FALSE(response.ok());
    EXPECT_TRUE(response.error().contains("cmd")) << response.error();
}

TEST_F(CommandPortTest, MalformedJsonIsAnError) {
    const auto response = exchange(post(R"({"cmd":)"));

    EXPECT_EQ(response.code, 200);
    EXPECT_FALSE(response.ok());
    EXPECT_TRUE(response.error().starts_with("malformed JSON at line 1"))
        << response.error();
}

TEST_F(CommandPortTest, PostWithoutContentLengthIs400) {
    const auto response = exchange("POST /rpc HTTP/1.1\r\n\r\n");

    EXPECT_EQ(response.code, 400);
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.error(), "POST needs Content-Length");
}

TEST_F(CommandPortTest, MalformedRequestLineIs400) {
    const auto response = exchange("hello there\r\n\r\n");

    EXPECT_EQ(response.code, 400);
    EXPECT_FALSE(response.ok());
}

TEST_F(CommandPortTest, UnknownPathIs404) {
    const auto response = exchange("GET /nope HTTP/1.1\r\n\r\n");

    EXPECT_EQ(response.code, 404);
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.error(), "no route for GET /nope");
}

// rejected on the header alone: the body never has to arrive
TEST_F(CommandPortTest, OversizedContentLengthIsRejectedWithoutBody) {
    const auto response =
        exchange("POST /rpc HTTP/1.1\r\nContent-Length: 9999999\r\n\r\n");

    EXPECT_EQ(response.code, 200);
    EXPECT_FALSE(response.ok());
    EXPECT_TRUE(response.error().contains("exceeds")) << response.error();
}

TEST_F(CommandPortTest, RequestSplitAcrossDrainsIsAccumulated) {
    const auto request = post(R"({"cmd":"ping"})");
    const auto cut = request.find("Content-Le") + 4;

    Client client(port.Port());
    client.Send(StrView(request).substr(0, cut));
    ASSERT_TRUE(pump(port, [&] { return port.Status().connectionCount == 1; }));
    EXPECT_FALSE(client.Closed());
    client.Send(StrView(request).substr(cut, request.size() - cut - 5));
    port.Drain();
    EXPECT_FALSE(client.Closed());
    client.Send(StrView(request).substr(request.size() - 5));

    const auto response = parseResponse(client.Receive(port));
    EXPECT_TRUE(response.ok()) << response.body;
}

TEST_F(CommandPortTest, VerbSeesItsArgsAndArgsDefaultToEmptyTable) {
    std::optional<i64> seen;
    bool sawTable = false;
    port.RegisterVerb("echo", [&](const DOM::Value& args, Reply reply) {
        sawTable = args.is_table();
        seen = args.get<i64>("value");
        reply.Ok(DOM::Value(seen.value_or(-1)));
    });

    auto response = exchange(post(R"({"cmd":"echo","args":{"value":42}})"));
    EXPECT_TRUE(response.ok()) << response.body;
    EXPECT_EQ(seen, 42);
    EXPECT_EQ(response.json.get<i64>("result"), 42);

    response = exchange(post(R"({"cmd":"echo"})"));
    EXPECT_TRUE(response.ok()) << response.body;
    EXPECT_TRUE(sawTable);
    EXPECT_EQ(seen, std::nullopt);
}

TEST_F(CommandPortTest, FlagVerbFiresBeforeTheReplyGoesOut) {
    bool fired = false;
    port.RegisterVerb("flag", [&](const DOM::Value&, Reply reply) {
        fired = true;
        reply.Ok(DOM::Value(DOM::Table{}));
    });

    const auto response = exchange(post(R"({"cmd":"flag"})"));

    EXPECT_TRUE(fired);
    EXPECT_TRUE(response.ok()) << response.body;
}

TEST_F(CommandPortTest, DeferredReplyCompletesOnALaterDrain) {
    std::optional<Reply> held;
    port.RegisterVerb("later", [&](const DOM::Value&, Reply reply) {
        held = std::move(reply);
    });

    Client client(port.Port());
    client.Send(post(R"({"cmd":"later"})"));
    ASSERT_TRUE(pump(port, [&] { return held.has_value(); }));
    EXPECT_FALSE(client.Closed());
    EXPECT_EQ(port.Status().pendingCount, 1u);

    held->Ok(DOM::Value(Str("done")));
    held.reset();

    const auto response = parseResponse(client.Receive(port));
    EXPECT_TRUE(response.ok()) << response.body;
    EXPECT_EQ(response.json.get<Str>("result"), "done");
    EXPECT_EQ(port.Status().pendingCount, 0u);
}

TEST_F(CommandPortTest, DeferredReplyTimesOutIntoAnError) {
    std::optional<Reply> held;
    port.RegisterVerb("never", [&](const DOM::Value&, Reply reply) {
        held = std::move(reply);
    });

    Client client(port.Port());
    client.Send(post(R"({"cmd":"never"})"));
    ASSERT_TRUE(pump(port, [&] { return held.has_value(); }));

    std::this_thread::sleep_for(PastTimeout);

    const auto response = parseResponse(client.Receive(port));
    EXPECT_FALSE(response.ok());
    EXPECT_TRUE(response.error().starts_with("no reply within"))
        << response.error();

    // answering after the timeout is a no-op, not a crash
    held->Ok(DOM::Value(true));
}

TEST_F(CommandPortTest, DroppedReplyIsAnError) {
    port.RegisterVerb("drop", [](const DOM::Value&, Reply) {});

    const auto response = exchange(post(R"({"cmd":"drop"})"));

    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.error(), "verb 'drop' dropped its reply");
}

TEST_F(CommandPortTest, ThrowingVerbIsAnError) {
    port.RegisterVerb("boom", [](const DOM::Value&, Reply) {
        throw std::runtime_error("kaboom");
    });

    const auto response = exchange(post(R"({"cmd":"boom"})"));

    EXPECT_EQ(response.code, 200);
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.error(), "verb 'boom' threw: kaboom");
}

TEST_F(CommandPortTest, IdleConnectionIsReaped) {
    Client client(port.Port());
    ASSERT_TRUE(pump(port, [&] { return port.Status().connectionCount == 1; }));
    EXPECT_FALSE(client.Closed());

    std::this_thread::sleep_for(PastTimeout);

    EXPECT_TRUE(pump(port, [&] { return client.Closed(); }));
    EXPECT_EQ(port.Status().connectionCount, 0u);
}

TEST_F(CommandPortTest, PeerThatHangsUpEarlyDoesNotDisturbTheNext) {
    {
        Client client(port.Port());
        client.Send(post(R"({"cmd":"ping"})"));
    }
    ASSERT_TRUE(pump(port, [&] { return port.Status().everConnected; }));
    EXPECT_TRUE(pump(port, [&] { return port.Status().connectionCount == 0; }));

    const auto response = exchange(post(R"({"cmd":"ping"})"));
    EXPECT_TRUE(response.ok()) << response.body;
}

TEST(CommandPortConfigTest, ForcedPortWithoutRetriesReportsBindFailure) {
    CommandPort first{CommandPortConfig{.port = 0}};
    ASSERT_NE(first.Port(), 0);

    CommandPort second{
        CommandPortConfig{.port = first.Port(), .portRetries = 0}
    };

    EXPECT_EQ(second.Port(), 0);
    EXPECT_EQ(second.Status().server, CommandPortStatus::Server::BindFailed);
    // a port that failed to bind is inert, never a crash
    second.Drain();
}

TEST(CommandPortConfigTest, RetriesStepPastAnOccupiedPort) {
    CommandPort first{CommandPortConfig{.port = 0}};
    ASSERT_NE(first.Port(), 0);

    CommandPort second{
        CommandPortConfig{.port = first.Port(), .portRetries = 1}
    };

    EXPECT_EQ(second.Port(), first.Port() + 1);
}

TEST_F(CommandPortTest, ListObjectsNamesEachExposure) {
    auto empty = exchange(post(R"({"cmd":"list_objects"})"));
    ASSERT_TRUE(empty.ok()) << empty.body;
    EXPECT_EQ(empty.json.at("result.objects")->asArray()->size(), 0u);

    PortProbe probe;
    port.Expose("probe", &probe, *GetDesc<PortProbe>());

    auto one = exchange(post(R"({"cmd":"list_objects"})"));
    ASSERT_TRUE(one.ok()) << one.body;
    EXPECT_EQ(one.json.get<Str>("result.objects[0].name"), "probe");
    EXPECT_EQ(one.json.get<Str>("result.objects[0].type"), "PortProbe");

    port.Unexpose("probe");
    auto gone = exchange(post(R"({"cmd":"list_objects"})"));
    ASSERT_TRUE(gone.ok()) << gone.body;
    EXPECT_EQ(gone.json.at("result.objects")->asArray()->size(), 0u);
}

TEST_F(CommandPortTest, DescribeCarriesMetadataInBand) {
    PortProbe probe;
    port.Expose("probe", &probe, *GetDesc<PortProbe>());

    auto response = exchange(post(R"({"cmd":"describe","args":{"target":"probe"}})"));
    ASSERT_TRUE(response.ok()) << response.body;
    const auto& json = response.json;
    EXPECT_EQ(json.get<Str>("result.type"), "PortProbe");

    const auto* properties = json.at("result.properties");
    ASSERT_TRUE(properties != nullptr && properties->is_array());
    ASSERT_EQ(properties->asArray()->size(), 5u);

    // the parent's property comes first
    EXPECT_EQ(json.get<Str>("result.properties[0].name"), "exposure");
    EXPECT_EQ(json.get<Str>("result.properties[0].type"), "f32");
    EXPECT_TRUE(json.at("result.properties[0].uiRange") == nullptr);

    EXPECT_EQ(json.get<Str>("result.properties[1].name"), "roughness");
    EXPECT_EQ(json.get<Vec2>("result.properties[1].uiRange"), Vec2(0.0f, 1.0f));
    EXPECT_EQ(json.get<Str>("result.properties[1].tooltip"), "microfacet spread");

    EXPECT_EQ(json.get<Str>("result.properties[2].type"), "Vec3");

    EXPECT_EQ(json.get<Str>("result.properties[3].type"), "PortBlend");
    std::vector<Str> names;
    json.forEach("result.properties[3].enumerators", [&](const DOM::Value& v) {
        names.push_back(*v.asString());
    });
    EXPECT_EQ(names, (std::vector<Str>{"Opaque", "Masked", "Additive"}));

    // a nested struct carries its own tree, under its registered name
    EXPECT_EQ(json.get<Str>("result.properties[4].type"), "PortNested");
    EXPECT_EQ(json.get<Str>("result.properties[4].properties[0].name"), "amount");

    auto unknown = exchange(post(R"({"cmd":"describe","args":{"target":"nope"}})"));
    EXPECT_FALSE(unknown.ok());
    EXPECT_EQ(unknown.error(), "unknown target 'nope'");

    auto missing = exchange(post(R"({"cmd":"describe"})"));
    EXPECT_FALSE(missing.ok());
    EXPECT_EQ(missing.error(), "\"target\" is missing or not a string");
}

TEST_F(CommandPortTest, GetPropertyReadsAPathOrTheWholeObject) {
    PortProbe probe;
    port.Expose("probe", &probe, *GetDesc<PortProbe>());

    auto tint = exchange(post(R"({"cmd":"get_property","args":{"target":"probe","path":"tint"}})"));
    ASSERT_TRUE(tint.ok()) << tint.body;
    EXPECT_EQ(tint.json.get<Vec3>("result.value"), Vec3(1.0f, 0.5f, 0.25f));

    auto amount = exchange(post(R"({"cmd":"get_property","args":{"target":"probe","path":"nested.amount"}})"));
    ASSERT_TRUE(amount.ok()) << amount.body;
    EXPECT_EQ(amount.json.get<f32>("result.value"), 0.25f);

    auto blend = exchange(post(R"({"cmd":"get_property","args":{"target":"probe","path":"blend"}})"));
    ASSERT_TRUE(blend.ok()) << blend.body;
    EXPECT_EQ(blend.json.get<Str>("result.value"), "Masked");

    // a struct path reads the whole struct
    auto nested = exchange(post(R"({"cmd":"get_property","args":{"target":"probe","path":"nested"}})"));
    ASSERT_TRUE(nested.ok()) << nested.body;
    EXPECT_EQ(nested.json.get<f32>("result.value.amount"), 0.25f);

    // no path reads the whole object, the parent's key flat beside the rest
    auto whole = exchange(post(R"({"cmd":"get_property","args":{"target":"probe"}})"));
    ASSERT_TRUE(whole.ok()) << whole.body;
    EXPECT_EQ(whole.json.get<f32>("result.value.exposure"), 1.0f);
    EXPECT_EQ(whole.json.get<f32>("result.value.roughness"), 0.5f);
    EXPECT_EQ(whole.json.get<f32>("result.value.nested.amount"), 0.25f);

    auto badPath = exchange(post(R"({"cmd":"get_property","args":{"target":"probe","path":"nested.nope"}})"));
    EXPECT_FALSE(badPath.ok());
    EXPECT_EQ(badPath.error(), "no property 'nope' on 'PortNested'");

    auto notAString = exchange(post(R"({"cmd":"get_property","args":{"target":"probe","path":3}})"));
    EXPECT_FALSE(notAString.ok());
    EXPECT_EQ(notAString.error(), "\"path\" is not a string");
}

TEST_F(CommandPortTest, SetPropertyWritesAndFiresDirtyOnce) {
    PortProbe probe;
    int dirty = 0;
    port.Expose("probe", &probe, *GetDesc<PortProbe>(), [&dirty] { ++dirty; });

    auto tint = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"tint","value":[1,0,0]}})"));
    ASSERT_TRUE(tint.ok()) << tint.body;
    EXPECT_EQ(probe.tint, Vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(dirty, 1);

    auto amount = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"nested.amount","value":0.75}})"));
    ASSERT_TRUE(amount.ok()) << amount.body;
    EXPECT_EQ(probe.nested.amount, 0.75f);
    EXPECT_EQ(dirty, 2);

    auto blend = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"blend","value":"Opaque"}})"));
    ASSERT_TRUE(blend.ok()) << blend.body;
    EXPECT_EQ(probe.blend, PortBlend::Opaque);
    EXPECT_EQ(dirty, 3);

    // the inherited leaf writes through to the base member
    auto exposure = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"exposure","value":4}})"));
    ASSERT_TRUE(exposure.ok()) << exposure.body;
    EXPECT_EQ(probe.exposure, 4.0f);
    EXPECT_EQ(dirty, 4);
}

TEST_F(CommandPortTest, SetPropertyErrorsAreLoudAndDoNotFire) {
    PortProbe probe;
    int dirty = 0;
    port.Expose("probe", &probe, *GetDesc<PortProbe>(), [&dirty] { ++dirty; });

    auto unknown = exchange(post(R"({"cmd":"set_property","args":{"target":"nope","path":"tint","value":1}})"));
    EXPECT_EQ(unknown.error(), "unknown target 'nope'");

    auto badPath = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"nested.nope","value":1}})"));
    EXPECT_EQ(badPath.error(), "no property 'nope' on 'PortNested'");

    auto mismatch = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"roughness","value":"text"}})"));
    EXPECT_EQ(mismatch.error(), "'roughness' expects f32");

    auto unknownName = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"blend","value":"Glow"}})"));
    EXPECT_EQ(unknownName.error(), "'blend' expects PortBlend");

    auto wholeStruct = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"nested","value":{"amount":1}}})"));
    EXPECT_EQ(wholeStruct.error(), "'nested' is a struct; set one of its properties");

    auto noValue = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","path":"tint"}})"));
    EXPECT_EQ(noValue.error(), "\"value\" is missing");

    auto noPath = exchange(post(R"({"cmd":"set_property","args":{"target":"probe","value":1}})"));
    EXPECT_EQ(noPath.error(), "\"path\" is missing or not a string");

    EXPECT_EQ(dirty, 0);
    EXPECT_EQ(probe.roughness, 0.5f);
    EXPECT_EQ(probe.blend, PortBlend::Masked);
    EXPECT_EQ(probe.nested.amount, 0.25f);
}

TEST_F(CommandPortTest, ExposingATypeNeverRegisteredStops) {
    struct NeverRegistered {
        f32 value = 0.0f;
    };
    NeverRegistered target;

    EXPECT_DEATH(
        port.Expose("unregistered", &target, *GetDesc<NeverRegistered>()),
        "a type that was never registered"
    );
}
