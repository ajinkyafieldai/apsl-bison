#include <bison/cli.hpp>

#include <array>
#include <cassert>
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

struct Client final : bison::cli::Client {
    bison::cli::RunResult result{bison::cli::RunResult::passed};
    std::string_view host{};
    std::string_view recipe{};
    bool called{};

    bison::cli::RunResult run(
        std::string_view observed_host,
        std::string_view observed_recipe,
        bison::cli::EventSink &) override {
        called = true;
        host = observed_host;
        recipe = observed_recipe;
        return result;
    }
};

} // namespace

int main() {
    {
        bison::cli::Command command{};
        char const *argv[] = {"run", "recipe.lua"};

        auto const result = bison::cli::parse(2, argv, command);

        assert(result == bison::cli::ParseResult::ok);
        assert(command.kind == bison::cli::CommandKind::run);
        assert(command.run.host == "bison.local");
        assert(command.run.recipe == "recipe.lua");
    }

    {
        bison::cli::Command command{};
        char const *argv[] = {
            "run",
            "--host",
            "bison-12.local",
            "recipes/smoke.lua",
        };

        auto const result = bison::cli::parse(4, argv, command);

        assert(result == bison::cli::ParseResult::ok);
        assert(command.run.host == "bison-12.local");
        assert(command.run.recipe == "recipes/smoke.lua");
    }

    {
        bison::cli::Command command{};
        char const *argv[] = {"run"};
        assert(
            bison::cli::parse(1, argv, command) ==
            bison::cli::ParseResult::invalid_arguments);
    }

    {
        bison::cli::Command command{};
        char const *argv[] = {"wat"};
        assert(
            bison::cli::parse(1, argv, command) ==
            bison::cli::ParseResult::invalid_arguments);
    }

    {
        bison::cli::Command command{};
        char const *argv[] = {"run", "--wat", "recipe.lua"};
        assert(
            bison::cli::parse(3, argv, command) ==
            bison::cli::ParseResult::invalid_arguments);
    }

    {
        bison::cli::Command command{};
        char const *argv[] = {"run", "recipe.lua"};
        assert(bison::cli::parse(2, argv, command) == bison::cli::ParseResult::ok);

        Sink sink{};
        Client client{};

        auto const exit = bison::cli::execute(command, client, sink);

        assert(exit == bison::cli::ExitCode::success);
        assert(client.called);
        assert(client.host == "bison.local");
        assert(client.recipe == "recipe.lua");
    }

    {
        bison::cli::Command command{};
        char const *argv[] = {"run", "recipe.lua"};
        assert(bison::cli::parse(2, argv, command) == bison::cli::ParseResult::ok);

        Sink sink{};
        Client client{};
        client.result = bison::cli::RunResult::failed;

        assert(
            bison::cli::execute(command, client, sink) ==
            bison::cli::ExitCode::run_failed);
    }

    {
        bison::cli::Command command{};
        char const *argv[] = {"run", "recipe.lua"};
        assert(bison::cli::parse(2, argv, command) == bison::cli::ParseResult::ok);

        Sink sink{};
        Client client{};
        client.result = bison::cli::RunResult::transport_error;

        assert(
            bison::cli::execute(command, client, sink) ==
            bison::cli::ExitCode::transport_error);
    }
}
