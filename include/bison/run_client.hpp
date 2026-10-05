#ifndef BISON_RUN_CLIENT_HPP_
#define BISON_RUN_CLIENT_HPP_

#include <bison/cli.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace bison::cli {

struct RecipeDigest final {
    std::array<std::uint8_t, 32U> bytes{};

    friend constexpr bool operator==(
        RecipeDigest const &,
        RecipeDigest const &) = default;
};

[[nodiscard]] RecipeDigest sha256(
    std::span<std::byte const> bytes) noexcept;

[[nodiscard]] std::array<char, 64U> hex(
    RecipeDigest const &digest) noexcept;

struct RunHandle final {
    std::uint64_t value{};
};

class RunTransport {
public:
    virtual bool upload_recipe(
        std::string_view host,
        std::string_view recipe_name,
        std::span<std::byte const> recipe,
        RecipeDigest const &digest,
        EventSink &sink) = 0;

    virtual bool start_run(
        std::string_view host,
        RecipeDigest const &digest,
        RunHandle &run,
        EventSink &sink) = 0;

    virtual RunResult stream_run(
        std::string_view host,
        RunHandle run,
        EventSink &sink) = 0;

    virtual ~RunTransport() = default;
};

class RunClient final : public Client {
public:
    explicit RunClient(RunTransport &transport) noexcept
        : transport_{transport} {}

    RunResult run(
        std::string_view host,
        std::string_view recipe_path,
        EventSink &sink) override;

private:
    RunTransport &transport_;
};

} // namespace bison::cli

#endif // BISON_RUN_CLIENT_HPP_
