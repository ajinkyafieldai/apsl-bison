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
        std::array<char, 128U> text{};
        std::size_t size{};
    };

    std::array<StoredEvent, 8U> events{};
    std::size_t count{};

    void emit(bison::cli::Event event) override {
        assert(count < events.size());
        assert(event.text.size() <= events[count].text.size());

        auto &stored = events[count++];
        stored.stream = event.stream;
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

    auto const path =
        std::filesystem::temp_directory_path() /
        "bison-server-integration.lua";

    {
        std::ofstream output{path, std::ios::binary};
        output << "print('hello from recipe')\n";
    }

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
        path.string(),
        sink);

    assert(result == bison::cli::RunResult::passed);
    assert(sink.count == 2U);
    assert(sink.events[0].stream == bison::cli::Stream::out);
    assert(sink.text(0U) == "recipe loaded");
    assert(sink.events[1].stream == bison::cli::Stream::out);
    assert(sink.text(1U) == "run started");
    assert(!state.active);
    assert(state.recipe_size > 0U);

    std::filesystem::remove(path);

    running.store(false);
    server_thread.join();
}
