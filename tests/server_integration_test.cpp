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

namespace {

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

    running.store(false);
    server_thread.join();
}
