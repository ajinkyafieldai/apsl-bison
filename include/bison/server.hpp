#ifndef BISON_SERVER_HPP_
#define BISON_SERVER_HPP_

#include <apsl/net/web_server.hpp>
#include <bison/run_client.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace bison::server {

struct State final {
    static constexpr std::size_t recipe_capacity = 32U * 1024U;

    std::array<std::byte, recipe_capacity> recipe{};
    std::size_t recipe_size{};
    cli::RecipeDigest digest{};
    std::uint64_t next_run{1U};
    std::uint64_t active_run{};
    bool active{};

    [[nodiscard]] bool upload(std::string_view body) {
        if (body.empty() || body.size() > recipe.size()) {
            return false;
        }

        for (std::size_t i = 0; i < body.size(); ++i) {
            recipe[i] = static_cast<std::byte>(
                static_cast<unsigned char>(body[i]));
        }

        recipe_size = body.size();
        digest = cli::sha256(
            std::span<std::byte const>{recipe.data(), recipe_size});
        active = false;
        active_run = 0U;
        return true;
    }

    [[nodiscard]] bool start(
        std::string_view requested_digest,
        std::uint64_t &run_id) {
        if (recipe_size == 0U) {
            return false;
        }

        auto const encoded = cli::hex(digest);
        auto const expected = std::string_view{
            encoded.data(),
            encoded.size()};

        if (requested_digest != expected || active) {
            return false;
        }

        run_id = next_run++;
        active_run = run_id;
        active = true;
        return true;
    }

    void finish() noexcept {
        active = false;
        active_run = 0U;
    }
};

inline State *state{};

struct UploadRecipe {
    static constexpr std::string_view Mime_Type{"application/x-lua"};
    using request_type = std::string_view;

    static std::optional<request_type> parse(std::string_view body) {
        if (body.empty()) {
            return std::nullopt;
        }
        return body;
    }

    static apsl::web::Response handle(
        apsl::web::Context,
        request_type body) {
        if (state == nullptr || !state->upload(body)) {
            return {
                400,
                apsl::web::ContentType::TextPlain,
                "invalid recipe\n"};
        }

        return {
            204,
            apsl::web::ContentType::TextPlain,
            ""};
    }
};

struct FixedBody final {
    std::array<char, 32U> storage{};
    std::size_t used{};

    [[nodiscard]] std::size_t size() const noexcept {
        return used;
    }

    [[nodiscard]] bool empty() const noexcept {
        return used == 0U;
    }

    [[nodiscard]] operator std::string_view() const noexcept {
        return {storage.data(), used};
    }
};

struct StartRunResult final {
    bool ok{};
    FixedBody body{};
};

struct StartRun {
    static constexpr std::string_view Mime_Type{"text/plain"};
    using request_type = std::string_view;

    static std::optional<request_type> parse(std::string_view body) {
        if (body.size() != 64U) {
            return std::nullopt;
        }
        return body;
    }

    static StartRunResult handle(
        apsl::web::Context,
        request_type digest) {
        StartRunResult result{};
        std::uint64_t run{};

        if (state == nullptr || !state->start(digest, run)) {
            return result;
        }

        auto const encoded = std::to_chars(
            result.body.storage.data(),
            result.body.storage.data() + result.body.storage.size(),
            run);

        if (encoded.ec != std::errc{}) {
            state->finish();
            return {};
        }

        result.ok = true;
        result.body.used = static_cast<std::size_t>(
            encoded.ptr - result.body.storage.data());
        return result;
    }

    static apsl::web::BasicResponse<FixedBody> serialize(
        StartRunResult result) {
        if (!result.ok) {
            FixedBody body{};
            constexpr std::string_view message{"run rejected\n"};
            for (std::size_t i = 0; i < message.size(); ++i) {
                body.storage[i] = message[i];
            }
            body.used = message.size();

            return {
                .status = 400,
                .content_type = apsl::web::ContentType::TextPlain,
                .body = body,
            };
        }

        return {
            .status = 201,
            .content_type = apsl::web::ContentType::TextPlain,
            .body = result.body,
        };
    }
};

struct RunEvents {
    static apsl::web::WebTask<512> handle(
        apsl::web::Context,
        apsl::web::WebSocket socket) {
        if (state == nullptr || !state->active) {
            if (socket.is_open()) {
                (void)co_await socket.send("err\tno active run");
            }
            if (socket.is_open()) {
                (void)co_await socket.send("result\tfailed");
            }
            co_return;
        }

        if (!co_await socket.send("out\trecipe loaded")) {
            state->finish();
            co_return;
        }

        if (!co_await socket.send("out\trun started")) {
            state->finish();
            co_return;
        }

        if (socket.is_open()) {
            (void)co_await socket.send("result\tpassed");
        }

        state->finish();
    }
};

constexpr auto router = apsl::web::routes(
    apsl::web::post<"/api/v1/recipes", UploadRecipe>(),
    apsl::web::post<"/api/v1/runs", StartRun>(),
    apsl::web::websocket<"/api/v1/run/events", RunEvents>());

} // namespace bison::server

#endif // BISON_SERVER_HPP_
