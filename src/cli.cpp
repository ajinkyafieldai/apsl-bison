#include <bison/cli.hpp>

namespace bison::cli {

ParseResult parse(
    int argc,
    char const *const *argv,
    Command &command) noexcept {
    command = {};

    if (argc <= 0) {
        return ParseResult::help;
    }

    auto const first = std::string_view{argv[0]};
    if (first == "-h" || first == "--help") {
        return ParseResult::help;
    }

    if (first != "run") {
        return ParseResult::invalid_arguments;
    }

    command.kind = CommandKind::run;

    auto index = std::size_t{1U};
    while (index < static_cast<std::size_t>(argc)) {
        auto const argument = std::string_view{argv[index]};

        if (argument == "-h" || argument == "--help") {
            return ParseResult::help;
        }

        if (argument == "--host") {
            if (index + 1U >= static_cast<std::size_t>(argc)) {
                return ParseResult::invalid_arguments;
            }
            command.run.host = argv[index + 1U];
            index += 2U;
            continue;
        }

        if (argument.starts_with("--")) {
            return ParseResult::invalid_arguments;
        }

        if (!command.run.recipe.empty()) {
            return ParseResult::invalid_arguments;
        }

        command.run.recipe = argument;
        ++index;
    }

    if (command.run.recipe.empty()) {
        return ParseResult::invalid_arguments;
    }

    return ParseResult::ok;
}

ExitCode execute(
    Command const &command,
    Client &client,
    EventSink &sink) {
    if (command.kind != CommandKind::run) {
        return ExitCode::usage;
    }

    switch (client.run(
        command.run.host,
        command.run.recipe,
        sink)) {
    case RunResult::passed:
        return ExitCode::success;
    case RunResult::failed:
    case RunResult::interrupted:
        return ExitCode::run_failed;
    case RunResult::transport_error:
        return ExitCode::transport_error;
    }

    return ExitCode::transport_error;
}

} // namespace bison::cli
