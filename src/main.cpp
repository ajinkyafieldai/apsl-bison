#include <bison/cli.hpp>

#include <cstdlib>
#include <iostream>
#include <span>
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

class UnavailableClient final : public bison::cli::Client {
public:
    bison::cli::RunResult run(
        std::string_view host,
        std::string_view recipe_path,
        bison::cli::EventSink &sink) override {
        sink.emit({
            .stream = bison::cli::Stream::err,
            .text = "bison: transport backend is not linked yet",
        });
        (void)host;
        (void)recipe_path;
        return bison::cli::RunResult::transport_error;
    }
};

} // namespace

int main(int argc, char **argv) {
    auto arguments = std::span<char const *const>{
        const_cast<char const *const *>(argv + 1),
        static_cast<std::size_t>(argc > 0 ? argc - 1 : 0)};

    bison::cli::Command command{};
    auto const parse_result = bison::cli::parse(arguments, command);

    if (parse_result == bison::cli::ParseResult::help) {
        std::cout << bison::cli::usage();
        return static_cast<int>(bison::cli::ExitCode::success);
    }

    if (parse_result == bison::cli::ParseResult::invalid_arguments) {
        std::cerr << bison::cli::usage();
        return static_cast<int>(bison::cli::ExitCode::usage);
    }

    TerminalSink sink{};
    UnavailableClient client{};
    return static_cast<int>(bison::cli::execute(command, client, sink));
}
