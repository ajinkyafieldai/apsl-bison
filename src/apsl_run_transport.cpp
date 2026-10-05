#include <bison/apsl_run_transport.hpp>

#include <apsl/core/coro.hpp>
#include <apsl/net/posix/web_transport.hpp>
#include <apsl/net/web_client.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <span>
#include <string_view>

namespace bison::cli {

namespace {

using WebClient =
    apsl::net::http::Client<apsl::net::posix::WebTransport, 2048U, 16384U>;

inline apsl::core::coro::StaticPool<1U, 4096U> task_frames;
using Task = apsl::core::coro::Task<4096U, task_frames>;

struct Endpoint final {
    apsl::net::Address address{
        apsl::net::Ipv4Address::any(),
        apsl::net::Port{80U}};
};

[[nodiscard]] bool resolve_endpoint(
    std::string_view authority,
    Endpoint &endpoint) {
    if (authority.empty()) {
        return false;
    }

    auto host = authority;
    std::uint16_t port = 80U;

    auto const colon = authority.rfind(':');
    if (colon != std::string_view::npos) {
        if (authority.find(':') != colon) {
            return false;
        }

        host = authority.substr(0U, colon);
        auto const port_text = authority.substr(colon + 1U);
        unsigned parsed_port{};
        auto const parsed = std::from_chars(
            port_text.data(),
            port_text.data() + port_text.size(),
            parsed_port);

        if (parsed.ec != std::errc{} ||
            parsed.ptr != port_text.data() + port_text.size() ||
            parsed_port == 0U ||
            parsed_port > 65535U) {
            return false;
        }

        port = static_cast<std::uint16_t>(parsed_port);
    }

    if (host.empty() || host.size() >= 256U) {
        return false;
    }

    std::array<char, 256U> hostname{};
    std::memcpy(hostname.data(), host.data(), host.size());
    hostname[host.size()] = '\0';

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *results{};
    if (::getaddrinfo(hostname.data(), nullptr, &hints, &results) != 0 ||
        results == nullptr) {
        if (results != nullptr) {
            ::freeaddrinfo(results);
        }
        return false;
    }

    auto const *address =
        reinterpret_cast<sockaddr_in const *>(results->ai_addr);

    endpoint.address = apsl::net::Address{
        apsl::net::Ipv4Address{ntohl(address->sin_addr.s_addr)},
        apsl::net::Port{port}};

    ::freeaddrinfo(results);
    return true;
}

void drive(WebClient &client, Task &task) {
    while (!task.done() || client.active()) {
        client.service();
        (void)::poll(nullptr, 0, 1);
    }
}

Task request_task(
    WebClient &client,
    apsl::net::http::Request request,
    unsigned expected_status,
    bool &ok) {
    auto response = co_await client.request(request);
    ok = response && response->status == expected_status;
}

Task start_task(
    WebClient &client,
    apsl::net::http::Request request,
    RunHandle &run,
    bool &ok) {
    auto response = co_await client.request(request);
    if (!response ||
        (response->status != 200U && response->status != 201U)) {
        ok = false;
        co_return;
    }

    std::uint64_t value{};
    auto const parsed = std::from_chars(
        response->body.data(),
        response->body.data() + response->body.size(),
        value);

    ok =
        parsed.ec == std::errc{} &&
        parsed.ptr == response->body.data() + response->body.size();

    if (ok) {
        run.value = value;
    }
}

Task stream_task(
    WebClient &client,
    std::string_view path,
    EventSink &sink,
    RunResult &result) {
    auto connection = co_await client.connect_websocket(path);
    if (!connection) {
        result = RunResult::transport_error;
        co_return;
    }

    while (connection->is_open()) {
        auto message = co_await connection->receive();
        if (!message) {
            result = RunResult::transport_error;
            co_return;
        }

        switch (decode_run_event(*message, sink)) {
        case WireEventResult::emitted:
            break;

        case WireEventResult::passed:
            result = RunResult::passed;
            (void)co_await connection->close();
            co_return;

        case WireEventResult::failed:
            result = RunResult::failed;
            (void)co_await connection->close();
            co_return;

        case WireEventResult::invalid:
            sink.emit({
                .stream = Stream::err,
                .text = "bison: invalid run event",
            });
            result = RunResult::transport_error;
            (void)co_await connection->close();
            co_return;
        }
    }

    result = RunResult::transport_error;
}

[[nodiscard]] bool create_client(
    std::string_view authority,
    Endpoint &endpoint) {
    return resolve_endpoint(authority, endpoint);
}

} // namespace

WireEventResult decode_run_event(
    std::string_view message,
    EventSink &sink) {
    auto const separator = message.find('\t');
    auto const kind = message.substr(0U, separator);
    auto const payload =
        separator == std::string_view::npos
            ? std::string_view{}
            : message.substr(separator + 1U);

    if (kind == "out" && separator != std::string_view::npos) {
        sink.emit({
            .stream = Stream::out,
            .text = payload,
        });
        return WireEventResult::emitted;
    }

    if (kind == "err" && separator != std::string_view::npos) {
        sink.emit({
            .stream = Stream::err,
            .text = payload,
        });
        return WireEventResult::emitted;
    }

    if (kind == "result" && payload == "passed") {
        return WireEventResult::passed;
    }

    if (kind == "result" && payload == "failed") {
        return WireEventResult::failed;
    }

    return WireEventResult::invalid;
}

bool ApslRunTransport::upload_recipe(
    std::string_view host,
    std::string_view recipe_name,
    std::span<std::byte const> recipe,
    RecipeDigest const &digest,
    EventSink &sink) {
    Endpoint endpoint{};
    if (!create_client(host, endpoint)) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: cannot resolve host",
        });
        return false;
    }

    auto const encoded_digest = hex(digest);
    auto const digest_text = std::string_view{
        encoded_digest.data(),
        encoded_digest.size()};

    std::array<apsl::net::http::Header, 2U> headers{{
        {
            .name = "X-Bison-Recipe-Name",
            .value = recipe_name,
        },
        {
            .name = "X-Bison-Recipe-SHA256",
            .value = digest_text,
        },
    }};

    auto const body = std::string_view{
        reinterpret_cast<char const *>(recipe.data()),
        recipe.size()};

    apsl::net::posix::WebTransport transport{endpoint.address};
    WebClient client{transport, host};

    bool ok{};
    auto task = request_task(
        client,
        apsl::net::http::Request{
            .method = "POST",
            .path = "/api/v1/recipes",
            .body = body,
            .content_type = "application/x-lua",
            .headers = headers,
        },
        204U,
        ok);

    if (!task) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: cannot allocate HTTP coroutine",
        });
        return false;
    }

    drive(client, task);
    if (!ok) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: recipe upload failed",
        });
    }
    return ok;
}

bool ApslRunTransport::start_run(
    std::string_view host,
    RecipeDigest const &digest,
    RunHandle &run,
    EventSink &sink) {
    Endpoint endpoint{};
    if (!create_client(host, endpoint)) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: cannot resolve host",
        });
        return false;
    }

    auto const encoded_digest = hex(digest);
    auto const digest_text = std::string_view{
        encoded_digest.data(),
        encoded_digest.size()};

    apsl::net::posix::WebTransport transport{endpoint.address};
    WebClient client{transport, host};

    bool ok{};
    auto task = start_task(
        client,
        apsl::net::http::Request{
            .method = "POST",
            .path = "/api/v1/runs",
            .body = digest_text,
            .content_type = "text/plain",
        },
        run,
        ok);

    if (!task) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: cannot allocate HTTP coroutine",
        });
        return false;
    }

    drive(client, task);
    if (!ok) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: run start failed",
        });
    }
    return ok;
}

RunResult ApslRunTransport::stream_run(
    std::string_view host,
    RunHandle run,
    EventSink &sink) {
    Endpoint endpoint{};
    if (!create_client(host, endpoint)) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: cannot resolve host",
        });
        return RunResult::transport_error;
    }

    (void)run;
    constexpr std::string_view path_text{"/api/v1/run/events"};

    apsl::net::posix::WebTransport transport{endpoint.address};
    WebClient client{transport, host};

    auto result = RunResult::transport_error;
    auto task = stream_task(
        client,
        path_text,
        sink,
        result);

    if (!task) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: cannot allocate WebSocket coroutine",
        });
        return RunResult::transport_error;
    }

    drive(client, task);
    return result;
}

} // namespace bison::cli
