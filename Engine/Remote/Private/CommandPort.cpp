#include "CommandPort.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <exception>
#include <format>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "JsonLoader.hpp"
#include "LogLocal.hpp"
#include "RangeUtil.hpp"
#include "ReflectionVerbs.hpp"
#include "Socket.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        constexpr usize ReceiveChunkBytes = 64 * 1024;
        constexpr usize MaxHeadBytes = 16 * 1024;
        constexpr int ListenBacklog = 8;
        constexpr StrView HeadTerminator = "\r\n\r\n";

        enum class ConnectionState : u8 { Reading, Pending, Writing };

        struct Connection {
            Socket::Handle socket = Socket::Invalid;
            ConnectionState state = ConnectionState::Reading;
            RequestId request = 0;
            Str verb;
            bool abandoned = false;
            bool peerClosed = false;
            bool closed = false;
            Str in;
            Str out;
            usize outSent = 0;
            Clock::time_point lastActivity;
        };

        struct RequestHead {
            Str method;
            Str path;
            usize bytes = 0;
            std::optional<usize> contentLength;
        };

        enum class HeadParse : u8 { Incomplete, Complete, Malformed };

        StrView trimmed(StrView text) {
            while(!text.empty() &&
                  std::isspace(static_cast<unsigned char>(text.front())))
                text.remove_prefix(1);
            while(!text.empty() &&
                  std::isspace(static_cast<unsigned char>(text.back())))
                text.remove_suffix(1);

            return text;
        }

        HeadParse parseHead(StrView in, RequestHead& head, Str& error) {
            const auto end = in.find(HeadTerminator);
            if(end == StrView::npos)
                return HeadParse::Incomplete;

            head.bytes = end + HeadTerminator.size();

            const auto lineEnd = in.find("\r\n");
            const auto requestLine = in.substr(0, lineEnd);
            const auto methodEnd = requestLine.find(' ');
            const auto targetEnd = requestLine.find(' ', methodEnd + 1);
            if(methodEnd == StrView::npos || targetEnd == StrView::npos) {
                error = "malformed request line";

                return HeadParse::Malformed;
            }
            if(!requestLine.substr(targetEnd + 1).starts_with("HTTP/1.")) {
                error = "unsupported HTTP version";

                return HeadParse::Malformed;
            }

            head.method = Str(requestLine.substr(0, methodEnd));
            auto target =
                requestLine.substr(methodEnd + 1, targetEnd - methodEnd - 1);
            head.path = Str(target.substr(0, target.find('?')));

            auto rest = in.substr(lineEnd + 2, end - lineEnd - 2);
            while(!rest.empty()) {
                const auto next = rest.find("\r\n");
                const auto line = rest.substr(0, next);
                rest =
                    next == StrView::npos ? StrView{} : rest.substr(next + 2);

                const auto colon = line.find(':');
                if(colon == StrView::npos)
                    continue;

                const auto key = trimmed(line.substr(0, colon));
                if(!equalsIgnoringCase(key, "content-length"))
                    continue;

                head.contentLength =
                    parseNumber<usize>(trimmed(line.substr(colon + 1)));
                if(!head.contentLength) {
                    error = "malformed Content-Length";

                    return HeadParse::Malformed;
                }
            }

            return HeadParse::Complete;
        }

        // nlohmann: "[json.exception.parse_error.101] parse error at line 1,
        // column 8: syntax error while parsing value - unexpected ..."
        Str briefParseError(StrView what) {
            if(what.starts_with('[')) {
                if(const auto close = what.find("] "); close != StrView::npos)
                    what.remove_prefix(close + 2);
            }
            what = what.substr(0, what.find(';'));
            consumePrefix(what, "parse error ");

            const auto colon = what.find(": ");
            if(colon == StrView::npos)
                return Str(what);

            auto detail = what.substr(colon + 2);
            if(const auto dash = detail.rfind(" - "); dash != StrView::npos)
                detail.remove_prefix(dash + 3);

            return std::format("{}: {}", what.substr(0, colon), detail);
        }

        Str okBody(DOM::Value result) {
            DOM::Table table;
            table.emplace("ok", true);
            table.emplace("result", std::move(result));

            return emitJson(std::move(table));
        }

        Str errorBody(Str message) {
            DOM::Table table;
            table.emplace("ok", false);
            table.emplace("error", std::move(message));

            return emitJson(std::move(table));
        }

        CStr reasonOf(int code) noexcept {
            switch(code) {
            case 200:
                return "OK";
            case 400:
                return "Bad Request";
            case 404:
                return "Not Found";
            default:
                return "Unknown";
            }
        }

        Str httpResponse(int code, StrView body) {
            return std::format(
                "HTTP/1.1 {} {}\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: {}\r\n"
                "Connection: close\r\n"
                "\r\n"
                "{}",
                code,
                reasonOf(code),
                body.size(),
                body
            );
        }

        f64 secondsSince(
            Clock::time_point then,
            Clock::time_point now
        ) noexcept {
            return std::chrono::duration<f64>(now - then).count();
        }
    }

    class CommandPortImpl
        : public std::enable_shared_from_this<CommandPortImpl> {
    private:
        struct Verb {
            VerbKeys keys;
            VerbHandler handler;
        };
        using Verbs = std::unordered_map<Str, Verb>;
        using Connections = std::vector<Connection>;

        CommandPortConfig config;
        Verbs verbs;
        Exposures exposures;
        Connections connections;
        std::vector<char> receiveBuffer;
        Socket::Handle listener = Socket::Invalid;
        CommandPortStatus status;
        RequestId nextRequest = 1;

    public:
        ~CommandPortImpl();
        CROWY_DECLARE_PINNED(CommandPortImpl)

        explicit CommandPortImpl(const CommandPortConfig& config);

        void RegisterVerb(Str name, VerbKeys keys, VerbHandler handler);
        void Expose(Str name, PropertyTarget target, DirtyCallback onDirty);
        void Unexpose(StrView name);
        void Drain();

        CommandPortStatus Status() const {
            auto snapshot = status;
            snapshot.connectionCount = static_cast<u32>(connections.size());
            snapshot.pendingCount = static_cast<u32>(
                std::ranges::count_if(connections, [](const Connection& c) {
                    return c.state == ConnectionState::Pending;
                })
            );

            return snapshot;
        }
        u16 Port() const noexcept { return status.port; }

        void Complete(RequestId id, Str body);
        void Abandon(RequestId id) noexcept;

    private:
        void listen();
        void acceptConnections(Clock::time_point now);
        void readConnection(Connection&, Clock::time_point now);
        void tryDispatch(Connection&, Clock::time_point now);
        void rpc(Connection&, StrView body, Clock::time_point now);
        void listVerbs(Connection&, Clock::time_point now);
        void sweepPending(Clock::time_point now);
        void writeConnection(Connection&, Clock::time_point now);
        void respond(Connection&, int code, Str body, Clock::time_point now);
        Connection* find(RequestId id) noexcept;
        std::vector<Str> verbNames() const;
    };

    CommandPortImpl::CommandPortImpl(const CommandPortConfig& config)
        : config(config), receiveBuffer(ReceiveChunkBytes) {
        RegisterVerb(
            "list_objects",
            {},
            [this](const DOM::Value& args, Reply reply) {
                listObjects(exposures, args, std::move(reply));
            }
        );
        RegisterVerb(
            "describe",
            {"target"},
            [this](const DOM::Value& args, Reply reply) {
                describeObject(exposures, args, std::move(reply));
            }
        );
        RegisterVerb(
            "get_property",
            {"target", "path"},
            [this](const DOM::Value& args, Reply reply) {
                getProperty(exposures, args, std::move(reply));
            }
        );
        RegisterVerb(
            "set_property",
            {"target", "path", "value"},
            [this](const DOM::Value& args, Reply reply) {
                setProperty(exposures, args, std::move(reply));
            }
        );

        listen();
    }

    CommandPortImpl::~CommandPortImpl() {
        for(auto& c: connections) {
            if(c.state == ConnectionState::Pending) {
                const auto response =
                    httpResponse(200, errorBody("shutting down"));
                Socket::sendSome(c.socket, response.data(), response.size());
            }
            Socket::closeSocket(c.socket);
        }
        if(listener != Socket::Invalid)
            Socket::closeSocket(listener);
    }

    void CommandPortImpl::listen() {
        Socket::initialize();

        const u32 base = config.port;
        const u32 tries = base == 0 ? 1 : 1u + config.portRetries;
        for(u32 i = 0; i < tries && base + i <= 0xFFFF; ++i) {
            const auto port = static_cast<u16>(base + i);
            const auto socket = Socket::createTcp();
            if(socket == Socket::Invalid)
                break;

            Socket::setReuseAddress(socket);
            if(Socket::setNonBlocking(socket) &&
               Socket::bindLoopback(socket, port) &&
               Socket::listenOn(socket, ListenBacklog)) {
                listener = socket;
                status.port = Socket::boundPort(socket);
                status.server = CommandPortStatus::Server::Listening;
                LOG_INFO("listening on 127.0.0.1:{}", status.port);

                return;
            }
            Socket::closeSocket(socket);
        }

        status.server = CommandPortStatus::Server::BindFailed;
        LOG_WARN(
            "could not bind 127.0.0.1:{}{}; running without a port",
            base,
            tries > 1 ? std::format("..{}", base + tries - 1) : Str{}
        );
    }

    void CommandPortImpl::RegisterVerb(
        Str name,
        VerbKeys keys,
        VerbHandler handler
    ) {
        CROWY_ASSERT(!verbs.contains(name), "verb registered twice");
        verbs.emplace(
            std::move(name),
            Verb{std::move(keys), std::move(handler)}
        );
    }

    void CommandPortImpl::Expose(
        Str name,
        PropertyTarget target,
        DirtyCallback onDirty
    ) {
        CROWY_ASSERT(!exposures.contains(name), "target exposed twice");
        CROWY_ASSERT(
            !target.Desc().name.empty(),
            "a type that was never registered"
        );
        exposures.emplace(
            std::move(name),
            Exposure{.target = std::move(target), .onDirty = std::move(onDirty)}
        );
    }

    void CommandPortImpl::Unexpose(StrView name) {
        if(const auto it = exposures.find(name); it != exposures.end()) {
            exposures.erase(it);
        }
    }

    void CommandPortImpl::Drain() {
        ++status.drainCount;
        if(listener == Socket::Invalid)
            return;

        const auto now = Clock::now();

        acceptConnections(now);
        for(auto& c: connections) {
            if(c.state == ConnectionState::Reading)
                readConnection(c, now);
        }
        sweepPending(now);
        for(auto& c: connections) {
            if(c.state == ConnectionState::Writing && !c.closed)
                writeConnection(c, now);
        }

        for(const auto& c: connections) {
            if(c.closed)
                Socket::closeSocket(c.socket);
        }
        std::erase_if(connections, [](const Connection& c) {
            return c.closed;
        });
    }

    void CommandPortImpl::acceptConnections(Clock::time_point now) {
        while(connections.size() < config.maxConnections) {
            const auto socket = Socket::acceptOne(listener);
            if(socket == Socket::Invalid) {
                if(!Socket::wouldBlock())
                    LOG_WARN("accept failed ({})", Socket::lastError());

                return;
            }

            Socket::setNonBlocking(socket);
            Socket::setNoSigPipe(socket);
            connections.push_back(
                Connection{.socket = socket, .lastActivity = now}
            );
            status.everConnected = true;
        }
    }

    void CommandPortImpl::readConnection(Connection& c, Clock::time_point now) {
        while(true) {
            const auto received = Socket::receiveSome(
                c.socket,
                receiveBuffer.data(),
                receiveBuffer.size()
            );
            if(received > 0) {
                c.in.append(receiveBuffer.data(), static_cast<usize>(received));
                c.lastActivity = now;
                if(c.in.size() > MaxHeadBytes + config.maxBodyBytes)
                    break;

                continue;
            }
            if(received == 0) {
                c.peerClosed = true;
                break;
            }
            if(!Socket::wouldBlock()) {
                c.closed = true;

                return;
            }
            break;
        }

        tryDispatch(c, now);

        if(c.state == ConnectionState::Reading) {
            if(c.in.size() > MaxHeadBytes + config.maxBodyBytes)
                respond(c, 400, errorBody("request too large"), now);
            else if(c.peerClosed)
                c.closed = true;
            else if(
                secondsSince(c.lastActivity, now) > config.idleTimeoutSeconds
            )
                c.closed = true;
        }
    }

    void CommandPortImpl::tryDispatch(Connection& c, Clock::time_point now) {
        RequestHead head;
        Str error;
        switch(parseHead(c.in, head, error)) {
        case HeadParse::Incomplete:
            if(c.in.size() > MaxHeadBytes)
                respond(c, 400, errorBody("request head too large"), now);

            return;
        case HeadParse::Malformed:
            respond(c, 400, errorBody(std::move(error)), now);

            return;
        case HeadParse::Complete:
            break;
        }

        if(head.method == "GET" && head.path == "/") {
            listVerbs(c, now);

            return;
        }
        if(head.method != "POST" || head.path != "/rpc") {
            respond(
                c,
                404,
                errorBody(
                    std::format("no route for {} {}", head.method, head.path)
                ),
                now
            );

            return;
        }
        if(!head.contentLength) {
            respond(c, 400, errorBody("POST needs Content-Length"), now);

            return;
        }
        if(*head.contentLength > config.maxBodyBytes) {
            respond(
                c,
                200,
                errorBody(
                    std::format(
                        "body of {} bytes exceeds the {} byte cap",
                        *head.contentLength,
                        config.maxBodyBytes
                    )
                ),
                now
            );

            return;
        }
        if(c.in.size() < head.bytes + *head.contentLength)
            return;

        rpc(c, StrView(c.in).substr(head.bytes, *head.contentLength), now);
    }

    void CommandPortImpl::listVerbs(Connection& c, Clock::time_point now) {
        const auto names = verbNames();

        DOM::Array list;
        list.reserve(names.size());
        for(const auto& name: names)
            list.emplace_back(name);

        DOM::Table arguments;
        for(const auto& name: names) {
            DOM::Array keys;
            for(const auto& key: verbs.at(name).keys)
                keys.emplace_back(key);
            arguments.emplace(name, std::move(keys));
        }

        DOM::Table result;
        result.emplace("verbs", std::move(list));
        result.emplace("arguments", std::move(arguments));
        respond(c, 200, okBody(std::move(result)), now);
    }

    void CommandPortImpl::rpc(
        Connection& c,
        StrView body,
        Clock::time_point now
    ) {
        DOM::Value doc;
        try {
            doc = parseJsonString(body);
        }
        catch(const std::exception& e) {
            respond(
                c,
                200,
                errorBody(
                    std::format("malformed JSON {}", briefParseError(e.what()))
                ),
                now
            );

            return;
        }

        const auto cmd = doc.get<Str>("cmd");
        if(!cmd) {
            respond(
                c,
                200,
                errorBody("\"cmd\" is missing or not a string"),
                now
            );

            return;
        }
        // a misspelt "args" would otherwise run the verb with none
        constexpr std::array<StrView, 2> RequestKeys{"cmd", "args"};
        if(const auto unknown = stray(doc.asTable()->keys(), RequestKeys);
           !unknown.empty()) {
            respond(
                c,
                200,
                errorBody(
                    std::format(
                        "a request takes cmd and args; got {}",
                        join(unknown, ", ")
                    )
                ),
                now
            );

            return;
        }
        const auto verb = verbs.find(*cmd);
        if(verb == verbs.end()) {
            respond(
                c,
                200,
                errorBody(
                    std::format(
                        "unknown verb '{}' (one of {})",
                        *cmd,
                        join(verbNames(), ", ")
                    )
                ),
                now
            );

            return;
        }

        static const DOM::Value noArgs{DOM::Table{}};
        const auto* args = doc.at("args");
        if(args != nullptr && args->asTable() == nullptr) {
            respond(c, 200, errorBody("\"args\" is not an object"), now);

            return;
        }
        // a key the verb does not read would be dropped without a word
        const auto& keys = verb->second.keys;
        const auto unknown = args != nullptr
                                 ? stray(args->asTable()->keys(), keys)
                                 : std::vector<Str>{};
        if(!unknown.empty()) {
            auto message = keys.empty()
                               ? std::format(
                                     "{} takes no arguments; got {}",
                                     *cmd,
                                     join(unknown, ", ")
                                 )
                               : std::format(
                                     "{} does not take {}; it takes {}",
                                     *cmd,
                                     join(unknown, ", "),
                                     join(keys, ", ")
                                 );
            respond(c, 200, errorBody(std::move(message)), now);

            return;
        }

        const auto id = nextRequest++;
        c.request = id;
        c.verb = *cmd;
        c.state = ConnectionState::Pending;
        c.lastActivity = now;
        status.lastVerb = *cmd;
        status.lastVerbDrain = status.drainCount;

        try {
            verb->second.handler(
                args != nullptr ? *args : noArgs,
                Reply(weak_from_this(), id)
            );
        }
        catch(const std::exception& e) {
            Complete(
                id,
                errorBody(std::format("verb '{}' threw: {}", *cmd, e.what()))
            );
        }
        catch(...) {
            Complete(id, errorBody(std::format("verb '{}' threw", *cmd)));
        }

        if(auto* pending = find(id); pending != nullptr && pending->abandoned)
            Complete(
                id,
                errorBody(std::format("verb '{}' dropped its reply", *cmd))
            );
    }

    void CommandPortImpl::sweepPending(Clock::time_point now) {
        for(auto& c: connections) {
            if(c.state != ConnectionState::Pending)
                continue;

            if(c.abandoned) {
                Complete(
                    c.request,
                    errorBody(
                        std::format("verb '{}' dropped its reply", c.verb)
                    )
                );
            }
            else if(
                secondsSince(c.lastActivity, now) > config.pendingTimeoutSeconds
            ) {
                Complete(
                    c.request,
                    errorBody(
                        std::format(
                            "no reply within {:.1f} s",
                            config.pendingTimeoutSeconds
                        )
                    )
                );
            }
        }
    }

    void CommandPortImpl::writeConnection(
        Connection& c,
        Clock::time_point now
    ) {
        while(c.outSent < c.out.size()) {
            const auto sent = Socket::sendSome(
                c.socket,
                c.out.data() + c.outSent,
                c.out.size() - c.outSent
            );
            if(sent > 0) {
                c.outSent += static_cast<usize>(sent);
                c.lastActivity = now;

                continue;
            }
            if(sent < 0 && Socket::wouldBlock()) {
                if(secondsSince(c.lastActivity, now) >
                   config.idleTimeoutSeconds) {
                    status.lastReplyFailed = true;
                    c.closed = true;
                }

                return;
            }

            status.lastReplyFailed = true;
            c.closed = true;

            return;
        }

        status.lastReplyFailed = false;
        c.closed = true;
    }

    void CommandPortImpl::respond(
        Connection& c,
        int code,
        Str body,
        Clock::time_point now
    ) {
        c.out = httpResponse(code, body);
        c.outSent = 0;
        c.state = ConnectionState::Writing;
        c.lastActivity = now;
    }

    Connection* CommandPortImpl::find(RequestId id) noexcept {
        const auto it =
            std::ranges::find_if(connections, [id](const Connection& c) {
                return c.request == id;
            });

        return it != connections.end() ? &*it : nullptr;
    }

    std::vector<Str> CommandPortImpl::verbNames() const {
        auto names =
            verbs | std::views::keys | std::ranges::to<std::vector<Str>>();
        std::ranges::sort(names);

        return names;
    }

    void CommandPortImpl::Complete(RequestId id, Str body) {
        auto* c = find(id);
        if(c == nullptr || c->state != ConnectionState::Pending)
            return;

        respond(*c, 200, std::move(body), Clock::now());
    }

    void CommandPortImpl::Abandon(RequestId id) noexcept {
        if(auto* c = find(id);
           c != nullptr && c->state == ConnectionState::Pending)
            c->abandoned = true;
    }

    Reply::Reply(std::weak_ptr<CommandPortImpl> port, RequestId id)
        : port(std::move(port)), id(id) {}

    Reply::~Reply() {
        abandon();
    }

    Reply::Reply(Reply&& other) noexcept
        : port(std::move(other.port)), id(std::exchange(other.id, 0)) {}

    Reply& Reply::operator=(Reply&& other) noexcept {
        if(this != &other) {
            abandon();
            port = std::move(other.port);
            id = std::exchange(other.id, 0);
        }

        return *this;
    }

    void Reply::abandon() noexcept {
        if(id == 0)
            return;

        if(auto impl = port.lock())
            impl->Abandon(id);
        id = 0;
        port.reset();
    }

    void Reply::Ok(DOM::Value result) {
        CROWY_ASSERT(id != 0, "Reply answered twice");

        if(auto impl = port.lock())
            impl->Complete(id, okBody(std::move(result)));
        id = 0;
        port.reset();
    }

    void Reply::Error(Str message) {
        CROWY_ASSERT(id != 0, "Reply answered twice");

        if(auto impl = port.lock())
            impl->Complete(id, errorBody(std::move(message)));
        id = 0;
        port.reset();
    }

    CommandPort::CommandPort(const CommandPortConfig& config)
        : impl(std::make_shared<CommandPortImpl>(config)) {}

    CommandPort::~CommandPort() = default;

    void CommandPort::RegisterVerb(
        Str name,
        VerbKeys keys,
        VerbHandler handler
    ) {
        impl->RegisterVerb(
            std::move(name),
            std::move(keys),
            std::move(handler)
        );
    }

    void CommandPort::Expose(
        Str name,
        PropertyTarget target,
        DirtyCallback onDirty
    ) {
        impl->Expose(std::move(name), std::move(target), std::move(onDirty));
    }

    void CommandPort::Expose(
        Str name,
        void* target,
        const TypeDesc& desc,
        DirtyCallback onDirty
    ) {
        impl->Expose(
            std::move(name),
            PropertyTarget::Fixed(target, desc),
            std::move(onDirty)
        );
    }

    void CommandPort::Unexpose(StrView name) {
        impl->Unexpose(name);
    }

    PortExposures::~PortExposures() {
        unexposeAll();
    }

    PortExposures::PortExposures(PortExposures&& other) noexcept
        : port(std::exchange(other.port, nullptr)),
          names(std::exchange(other.names, {})) {}

    PortExposures& PortExposures::operator=(PortExposures&& other) noexcept {
        if(this != &other) {
            unexposeAll();
            port = std::exchange(other.port, nullptr);
            names = std::exchange(other.names, {});
        }

        return *this;
    }

    void PortExposures::Expose(
        Str name,
        PropertyTarget target,
        DirtyCallback onDirty
    ) {
        if(port == nullptr)
            return;

        port->Expose(name, std::move(target), std::move(onDirty));
        names.push_back(std::move(name));
    }

    void PortExposures::Expose(
        Str name,
        void* target,
        const TypeDesc& desc,
        DirtyCallback onDirty
    ) {
        Expose(
            std::move(name),
            PropertyTarget::Fixed(target, desc),
            std::move(onDirty)
        );
    }

    void PortExposures::unexposeAll() noexcept {
        if(port != nullptr) {
            for(const auto& name: names)
                port->Unexpose(name);
        }
        names.clear();
    }

    void CommandPort::Drain() {
        impl->Drain();
    }

    CommandPortStatus CommandPort::Status() const {
        return impl->Status();
    }

    u16 CommandPort::Port() const noexcept {
        return impl->Port();
    }
}
