#ifndef BISON_CLI_HPP_
#define BISON_CLI_HPP_

#include <cstdint>
#include <string_view>

namespace bison::cli {

enum class CommandKind : std::uint8_t {
    help,
    run,
};

struct RunCommand final {
    std::string_view host{"bison.local"};
    std::string_view recipe{};
};

struct Command final {
    CommandKind kind{CommandKind::help};
    RunCommand run{};
};

enum class ParseResult : std::uint8_t {
    ok,
    help,
    invalid_arguments,
};

[[nodiscard]] ParseResult parse(
    int argc,
    char const *const *argv,
    Command &command) noexcept;

enum class Stream : std::uint8_t {
    out,
    err,
};

enum class EventKind : std::uint8_t {
    log,
    run_state,
};

enum class RunState : std::uint8_t {
    none,
    running,
    passed,
    failed,
    interrupted,
};

struct Event final {
    Stream stream{Stream::out};
    EventKind kind{EventKind::log};
    std::uint64_t run{};
    RunState state{RunState::none};
    std::string_view text{};
};

class EventSink {
public:
    virtual void emit(Event event) = 0;
    virtual ~EventSink() = default;
};

enum class RunResult : std::uint8_t {
    passed,
    failed,
    interrupted,
    transport_error,
};

class Client {
public:
    virtual RunResult run(
        std::string_view host,
        std::string_view recipe_path,
        EventSink &sink) = 0;

    virtual ~Client() = default;
};

enum class ExitCode : int {
    success = 0,
    run_failed = 1,
    usage = 2,
    transport_error = 3,
};

[[nodiscard]] ExitCode execute(
    Command const &command,
    Client &client,
    EventSink &sink);

[[nodiscard]] constexpr std::string_view usage() noexcept {
    return
        "usage:\n"
        "  bison run [--host HOST] <recipe.lua>\n";
}

} // namespace bison::cli

#endif // BISON_CLI_HPP_
