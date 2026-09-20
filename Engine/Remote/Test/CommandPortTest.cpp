#include <chrono>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include "CommandPort.hpp"
#include "JsonLoader.hpp"
#include "Socket.hpp"

using namespace Crowy;

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
    const auto response = exchange(post(R"({"cmd":"ping"})"));

    EXPECT_EQ(response.code, 200);
    EXPECT_TRUE(response.ok()) << response.body;
    EXPECT_EQ(response.json.get<bool>("result.pong"), true);

    const auto status = port.Status();
    EXPECT_TRUE(status.everConnected);
    EXPECT_EQ(status.lastVerb, "ping");
    EXPECT_EQ(status.lastVerbDrain, status.drainCount - 1);
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
    EXPECT_EQ(verbs, (std::vector<Str>{"alpha", "ping"}));
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
