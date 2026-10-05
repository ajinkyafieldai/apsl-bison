#ifndef BISON_APSL_RUN_TRANSPORT_HPP_
#define BISON_APSL_RUN_TRANSPORT_HPP_

#include <bison/run_client.hpp>

#include <string_view>

namespace bison::cli {

enum class WireEventResult {
    emitted,
    passed,
    failed,
    invalid,
};

[[nodiscard]] WireEventResult decode_run_event(
    std::string_view message,
    EventSink &sink);

class ApslRunTransport final : public RunTransport {
public:
    bool upload_recipe(
        std::string_view host,
        std::string_view recipe_name,
        std::span<std::byte const> recipe,
        RecipeDigest const &digest,
        EventSink &sink) override;

    bool start_run(
        std::string_view host,
        RecipeDigest const &digest,
        RunHandle &run,
        EventSink &sink) override;

    RunResult stream_run(
        std::string_view host,
        RunHandle run,
        EventSink &sink) override;
};

} // namespace bison::cli

#endif // BISON_APSL_RUN_TRANSPORT_HPP_
