#include <bison/cli.hpp>
#include <bison/run_client.hpp>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

class TerminalSink final : public bison::cli::EventSink {
public:
    void emit(bison::cli::Event event) override {
        auto &stream =
            event.stream == bison::cli::Stream::err
                ? std::cerr
                : std::cout;

        stream << event.text;
        if (!event.text.ends_with('\n')) {
            stream << '\n';
        }
    }
};

class UnavailableTransport final : public bison::cli::RunTransport {
public:
    bool upload_recipe(
        std::string_view,
        std::string_view,
        std::span<std::byte const>,
        bison::cli::RecipeDigest const &,
        bison::cli::EventSink &sink) override {
        sink.emit({
            .stream = bison::cli::Stream::err,
            .text = "bison: HTTP/WebSocket transport backend is not linked yet",
        });
        return false;
    }

    bool start_run(
        std::string_view,
        bison::cli::RecipeDigest const &,
        bison::cli::RunHandle &,
        bison::cli::EventSink &) override {
        return false;
    }

    bison::cli::RunResult stream_run(
        std::string_view,
        bison::cli::RunHandle,
        bison::cli::EventSink &) override {
        return bison::cli::RunResult::transport_error;
    }
};

} // namespace

int main(int argc, char **argv) {
    bison::cli::Command command{};
    auto const parse_result = bison::cli::parse(
        argc > 0 ? argc - 1 : 0,
        argc > 0 ? argv + 1 : nullptr,
        command);

    if (parse_result == bison::cli::ParseResult::help) {
        std::cout << bison::cli::usage();
        return static_cast<int>(bison::cli::ExitCode::success);
    }

    if (parse_result == bison::cli::ParseResult::invalid_arguments) {
        std::cerr << bison::cli::usage();
        return static_cast<int>(bison::cli::ExitCode::usage);
    }

    TerminalSink sink{};
    UnavailableTransport transport{};
    bison::cli::RunClient client{transport};
    return static_cast<int>(bison::cli::execute(command, client, sink));
}
