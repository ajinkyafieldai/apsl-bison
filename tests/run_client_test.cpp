#include <bison/apsl_run_transport.hpp>
#include <bison/run_client.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

struct Sink final : bison::cli::EventSink {
    std::array<bison::cli::Event, 8U> events{};
    std::size_t count{};

    void emit(bison::cli::Event event) override {
        assert(count < events.size());
        events[count++] = event;
    }
};

struct Transport final : bison::cli::RunTransport {
    enum class Step : std::uint8_t {
        none,
        uploaded,
        started,
        streamed,
    };

    Step step{Step::none};
    std::string_view host{};
    std::string recipe_name{};
    bison::cli::RecipeDigest digest{};
    std::size_t recipe_size{};
    bison::cli::RunHandle run{.value = 77U};
    bison::cli::RunResult stream_result{bison::cli::RunResult::passed};

    bool upload_recipe(
        std::string_view observed_host,
        std::string_view observed_name,
        std::span<std::byte const> recipe,
        bison::cli::RecipeDigest const &observed_digest,
        bison::cli::EventSink &) override {
        assert(step == Step::none);
        step = Step::uploaded;
        host = observed_host;
        recipe_name = observed_name;
        digest = observed_digest;
        recipe_size = recipe.size();
        return true;
    }

    bool start_run(
        std::string_view observed_host,
        bison::cli::RecipeDigest const &observed_digest,
        bison::cli::RunHandle &out_run,
        bison::cli::EventSink &) override {
        assert(step == Step::uploaded);
        assert(observed_host == host);
        assert(observed_digest == digest);
        step = Step::started;
        out_run = run;
        return true;
    }

    bison::cli::RunResult stream_run(
        std::string_view observed_host,
        bison::cli::RunHandle observed_run,
        bison::cli::EventSink &sink) override {
        assert(step == Step::started);
        assert(observed_host == host);
        assert(observed_run.value == run.value);
        step = Step::streamed;

        sink.emit({
            .stream = bison::cli::Stream::out,
            .text = "recipe: started",
        });

        return stream_result;
    }
};

[[nodiscard]] std::string digest_text(bison::cli::RecipeDigest const &digest) {
    auto const encoded = bison::cli::hex(digest);
    return std::string{encoded.data(), encoded.size()};
}

} // namespace

int main() {
    {
        constexpr std::array<std::byte, 3U> abc{
            std::byte{'a'},
            std::byte{'b'},
            std::byte{'c'},
        };

        auto const digest = bison::cli::sha256(abc);
        assert(
            digest_text(digest) ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad");
    }


    {
        Sink events{};

        assert(
            bison::cli::decode_run_event(
                "out\tbooting",
                events,
                {.value = 41U}) ==
            bison::cli::WireEventResult::emitted);
        assert(
            bison::cli::decode_run_event(
                "err\tfault",
                events,
                {.value = 41U}) ==
            bison::cli::WireEventResult::emitted);
        assert(
            bison::cli::decode_run_event(
                "state\t41\trunning",
                events,
                {.value = 41U}) ==
            bison::cli::WireEventResult::emitted);
        assert(
            bison::cli::decode_run_event(
                "state\t41\tpassed",
                events,
                {.value = 41U}) ==
            bison::cli::WireEventResult::passed);
        assert(
            bison::cli::decode_run_event(
                "state\t42\tfailed",
                events,
                {.value = 42U}) ==
            bison::cli::WireEventResult::failed);
        assert(
            bison::cli::decode_run_event(
                "wat",
                events,
                {.value = 41U}) ==
            bison::cli::WireEventResult::invalid);
        assert(
            bison::cli::decode_run_event(
                "state\t42\trunning",
                events,
                {.value = 41U}) ==
            bison::cli::WireEventResult::invalid);

        assert(events.count == 5U);
        assert(events.events[0].stream == bison::cli::Stream::out);
        assert(events.events[0].kind == bison::cli::EventKind::log);
        assert(events.events[0].text == "booting");
        assert(events.events[1].stream == bison::cli::Stream::err);
        assert(events.events[1].kind == bison::cli::EventKind::log);
        assert(events.events[1].text == "fault");

        assert(events.events[2].kind == bison::cli::EventKind::run_state);
        assert(events.events[2].run == 41U);
        assert(events.events[2].state == bison::cli::RunState::running);
        assert(events.events[3].kind == bison::cli::EventKind::run_state);
        assert(events.events[3].run == 41U);
        assert(events.events[3].state == bison::cli::RunState::passed);
        assert(events.events[4].kind == bison::cli::EventKind::run_state);
        assert(events.events[4].run == 42U);
        assert(events.events[4].state == bison::cli::RunState::failed);
    }

    auto const path =
        std::filesystem::temp_directory_path() /
        "bison-run-client-test.lua";

    {
        std::ofstream output{path, std::ios::binary};
        output << "print('hello')\n";
    }

    Sink sink{};
    Transport transport{};
    bison::cli::RunClient client{transport};

    auto const result = client.run(
        "bison.local",
        path.string(),
        sink);

    assert(result == bison::cli::RunResult::passed);
    assert(transport.step == Transport::Step::streamed);
    assert(transport.host == "bison.local");
    assert(transport.recipe_name == "bison-run-client-test.lua");
    assert(transport.recipe_size == 15U);
    assert(sink.count == 1U);
    assert(sink.events[0].stream == bison::cli::Stream::out);
    assert(sink.events[0].text == "recipe: started");

    std::filesystem::remove(path);
}
