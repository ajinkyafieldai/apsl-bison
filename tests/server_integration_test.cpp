#include <bison/apsl_run_transport.hpp>
#include <bison/run_client.hpp>
#include <bison/server.hpp>

#include <apsl/net/web_server.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

[[nodiscard]] std::string http_get(
    std::uint16_t port,
    std::string_view path) {
    auto const socket = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(socket >= 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    assert(::connect(
        socket,
        reinterpret_cast<sockaddr *>(&address),
        sizeof(address)) == 0);

    std::string request{
        "GET " + std::string{path} +
        " HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n"};

    auto offset = std::size_t{};
    while (offset < request.size()) {
        auto const written = ::send(
            socket,
            request.data() + offset,
            request.size() - offset,
            0);
        assert(written > 0);
        offset += static_cast<std::size_t>(written);
    }

    std::string response{};
    std::array<char, 1024U> buffer{};
    for (;;) {
        auto const received = ::recv(
            socket,
            buffer.data(),
            buffer.size(),
            0);
        if (received == 0) {
            break;
        }
        assert(received > 0);
        response.append(
            buffer.data(),
            static_cast<std::size_t>(received));
    }

    ::close(socket);
    return response;
}

struct Sink final : bison::cli::EventSink {
    struct StoredEvent {
        bison::cli::Stream stream{bison::cli::Stream::out};
        bison::cli::EventKind kind{bison::cli::EventKind::log};
        std::uint64_t run{};
        bison::cli::RunState state{bison::cli::RunState::none};
        std::array<char, 1024U> text{};
        std::size_t size{};
    };

    std::array<StoredEvent, 8U> events{};
    std::size_t count{};

    void emit(bison::cli::Event event) override {
        assert(count < events.size());
        assert(event.text.size() <= events[count].text.size());

        auto &stored = events[count++];
        stored.stream = event.stream;
        stored.kind = event.kind;
        stored.run = event.run;
        stored.state = event.state;
        stored.size = event.text.size();

        for (std::size_t i = 0; i < event.text.size(); ++i) {
            stored.text[i] = event.text[i];
        }
    }

    [[nodiscard]] std::string_view text(std::size_t index) const {
        return {
            events[index].text.data(),
            events[index].size};
    }
};

} // namespace

int main() {
    using namespace std::chrono_literals;

    bison::server::State state{};
    bison::server::state = &state;

    assert(state.snapshot_run_state() == "state\t0\tidle");
    assert(state.latest_run_record() == nullptr);
    assert(!bison::server::run_id_from_path("/api/v1/runs"));
    assert(!bison::server::run_id_from_path("/api/v1/runs/"));
    assert(!bison::server::run_id_from_path("/api/v1/runs/latest"));
    assert(!bison::server::run_id_from_path("/api/v1/runs/0"));
    assert(!bison::server::run_id_from_path("/api/v1/runs/12x"));
    auto const parsed_run =
        bison::server::run_id_from_path("/api/v1/runs/12");
    assert(parsed_run);
    assert(*parsed_run == 12U);

    apsl::web::ConnectionState endpoint_connection{};
    auto const no_record = bison::server::LatestRunRecord::handle(
        apsl::web::Context{endpoint_connection},
        {});
    assert(no_record.status == 404);
    auto const empty_list = bison::server::RunRecordList::handle(
        apsl::web::Context{endpoint_connection},
        {});
    assert(empty_list.status == 200);
    assert(empty_list.body.empty());

    apsl::web::Server<
        decltype(bison::server::router),
        4U,
        bison::server::State::recipe_capacity + 4096U>
        server{bison::server::router};

    assert(server.open(0U));
    auto const port = server.port();
    assert(port != 0U);

    std::atomic<bool> running{true};
    std::thread server_thread{[&] {
        while (running.load()) {
            server.run_once(1ms);
        }
    }};

    auto const hello_path =
        std::filesystem::path{BISON_SOURCE_DIR} /
        "examples/posix-project/hello_world.lua";
    auto const failing_path =
        std::filesystem::path{BISON_SOURCE_DIR} /
        "examples/posix-project/hello_world_failing.lua";

    assert(std::filesystem::exists(hello_path));
    assert(std::filesystem::exists(failing_path));

    std::array<char, 64U> host{};
    auto const size = std::snprintf(
        host.data(),
        host.size(),
        "127.0.0.1:%u",
        static_cast<unsigned>(port));
    assert(size > 0);
    assert(static_cast<std::size_t>(size) < host.size());

    Sink sink{};
    bison::cli::ApslRunTransport transport{};
    bison::cli::RunClient client{transport};

    auto const result = client.run(
        std::string_view{host.data(), static_cast<std::size_t>(size)},
        hello_path.string(),
        sink);

    assert(result == bison::cli::RunResult::passed);
    assert(sink.count == 3U);
    assert(sink.events[0].kind == bison::cli::EventKind::run_state);
    assert(sink.events[0].run == 1U);
    assert(sink.events[0].state == bison::cli::RunState::running);
    assert(sink.events[1].stream == bison::cli::Stream::out);
    assert(sink.events[1].kind == bison::cli::EventKind::log);
    assert(sink.text(1U) == "hello world");
    assert(sink.events[2].kind == bison::cli::EventKind::run_state);
    assert(sink.events[2].run == 1U);
    assert(sink.events[2].state == bison::cli::RunState::passed);
    assert(!state.active);
    assert(state.recipe_size > 0U);
    assert(state.snapshot_run_state() == "state\t1\tpassed");

    auto const *passed_record = state.latest_run_record();
    assert(passed_record != nullptr);
    assert(passed_record->run == 1U);
    assert(passed_record->phase == bison::server::RunPhase::passed);
    assert(passed_record->digest == state.digest);
    assert(passed_record->event_count == 1U);
    assert(!passed_record->events[0].error);
    auto const passed_event_text = std::string_view{
        passed_record->events[0].text.data(),
        passed_record->events[0].size};
    assert(passed_event_text == "hello world");

    auto const passed_response = bison::server::LatestRunRecord::handle(
        apsl::web::Context{endpoint_connection},
        {});
    assert(passed_response.status == 200);
    assert(passed_response.body.starts_with(
        "run\t1\nstate\tpassed\nstarted_ms\t"));
    assert(passed_response.body.find("\nfinished_ms\t") !=
           std::string_view::npos);
    assert(passed_response.body.find("\nduration_ms\t") !=
           std::string_view::npos);
    assert(passed_record->finished_ms >= passed_record->started_ms);
    assert(
        passed_record->duration_ms ==
        passed_record->finished_ms - passed_record->started_ms);
    assert(passed_response.body.find("\nout\thello world\n") !=
           std::string_view::npos);

    Sink failing_sink{};
    auto const failing_result = client.run(
        std::string_view{host.data(), static_cast<std::size_t>(size)},
        failing_path.string(),
        failing_sink);

    assert(failing_result == bison::cli::RunResult::failed);
    assert(failing_sink.count == 4U);
    assert(failing_sink.events[0].kind == bison::cli::EventKind::run_state);
    assert(failing_sink.events[0].run == 2U);
    assert(failing_sink.events[0].state == bison::cli::RunState::running);
    assert(failing_sink.events[1].stream == bison::cli::Stream::out);
    assert(failing_sink.events[1].kind == bison::cli::EventKind::log);
    assert(failing_sink.text(1U) == "hello world");
    assert(failing_sink.events[2].stream == bison::cli::Stream::err);
    assert(failing_sink.events[2].kind == bison::cli::EventKind::log);
    assert(failing_sink.text(2U).starts_with("recipe.lua:2: error: intentional failure"));
    assert(failing_sink.events[3].kind == bison::cli::EventKind::run_state);
    assert(failing_sink.events[3].run == 2U);
    assert(failing_sink.events[3].state == bison::cli::RunState::failed);
    assert(state.snapshot_run_state() == "state\t2\tfailed");

    auto const *failed_record = state.latest_run_record();
    assert(failed_record != nullptr);
    assert(failed_record->run == 2U);
    assert(failed_record->phase == bison::server::RunPhase::failed);
    assert(failed_record->digest == state.digest);
    assert(failed_record->event_count == 2U);
    assert(!failed_record->events[0].error);
    assert(failed_record->events[1].error);
    auto const failed_event_text = std::string_view{
        failed_record->events[1].text.data(),
        failed_record->events[1].size};
    assert(failed_event_text.starts_with(
        "recipe.lua:2: error: intentional failure"));

    auto const failed_snapshot = state.snapshot_latest_run_record();
    assert(failed_snapshot.starts_with(
        "run\t2\nstate\tfailed\nstarted_ms\t"));
    assert(failed_record->finished_ms >= failed_record->started_ms);
    assert(
        failed_record->duration_ms ==
        failed_record->finished_ms - failed_record->started_ms);
    assert(failed_snapshot.find("\nout\thello world\n") !=
           std::string_view::npos);
    assert(failed_snapshot.find(
        "\nerr\trecipe.lua:2: error: intentional failure") !=
        std::string_view::npos);
    assert(state.run_record_count == 2U);
    auto const *run_one = state.find_run_record(1U);
    assert(run_one != nullptr);
    assert(run_one->phase == bison::server::RunPhase::passed);
    auto const run_one_snapshot = state.snapshot_run_record(*run_one);
    assert(run_one_snapshot.starts_with(
        "run\t1\nstate\tpassed\nstarted_ms\t"));
    assert(state.find_run_record(99U) == nullptr);

    auto const list_http = http_get(port, "/api/v1/runs");
    assert(list_http.starts_with("HTTP/1.1 200 OK"));
    auto const failed_digest = bison::cli::hex(failed_record->digest);
    auto const passed_digest = bison::cli::hex(passed_record->digest);
    auto const failed_summary_prefix =
        std::string{"run\t2\tfailed\t"} +
        std::string{failed_digest.data(), failed_digest.size()} +
        "\t";
    auto const passed_summary_prefix =
        std::string{"run\t1\tpassed\t"} +
        std::string{passed_digest.data(), passed_digest.size()} +
        "\t";
    auto const failed_position = list_http.find(failed_summary_prefix);
    auto const passed_position = list_http.find(passed_summary_prefix);
    assert(failed_position != std::string::npos);
    assert(passed_position != std::string::npos);
    assert(failed_position < passed_position);

    auto const run_one_http = http_get(port, "/api/v1/runs/1");
    assert(run_one_http.starts_with("HTTP/1.1 200 OK"));
    assert(run_one_http.find(
        "run\t1\nstate\tpassed\nstarted_ms\t") !=
        std::string::npos);

    auto const missing_http = http_get(port, "/api/v1/runs/99");
    assert(missing_http.starts_with("HTTP/1.1 404 Not Found"));

    auto const malformed_http = http_get(port, "/api/v1/runs/nope");
    assert(malformed_http.starts_with("HTTP/1.1 400 Bad Request"));

    running.store(false);
    server_thread.join();

    bison::server::State history_state{};
    assert(history_state.upload("print('history')"));
    for (std::uint64_t run = 1U; run <= 6U; ++run) {
        history_state.active_run = run;
        history_state.active = true;
        history_state.finish((run % 2U) == 0U);
    }

    assert(
        history_state.run_record_count ==
        bison::server::State::run_record_capacity);
    auto const *latest_history = history_state.latest_run_record();
    assert(latest_history != nullptr);
    assert(latest_history->run == 6U);
    assert(latest_history->phase == bison::server::RunPhase::passed);
    assert(history_state.find_run_record(1U) == nullptr);
    assert(history_state.find_run_record(2U) == nullptr);
    for (std::uint64_t run = 3U; run <= 6U; ++run) {
        auto const *record = history_state.find_run_record(run);
        assert(record != nullptr);
        assert(record->run == run);
    }

    auto const history_list = history_state.snapshot_run_records();
    auto const run6 = history_list.find("run\t6\tpassed\t");
    auto const run5 = history_list.find("run\t5\tfailed\t");
    auto const run4 = history_list.find("run\t4\tpassed\t");
    auto const run3 = history_list.find("run\t3\tfailed\t");
    assert(run6 != std::string_view::npos);
    assert(run5 != std::string_view::npos);
    assert(run4 != std::string_view::npos);
    assert(run3 != std::string_view::npos);
    assert(run6 < run5);
    assert(run5 < run4);
    assert(run4 < run3);
}
