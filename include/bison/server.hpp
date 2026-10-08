#ifndef BISON_SERVER_HPP_
#define BISON_SERVER_HPP_

#include <apsl/net/web_server.hpp>
#include <apsl/lua/runtime.hpp>
#include <bison/run_client.hpp>

#include <array>
#include <algorithm>
#include <cstring>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace bison::server {

enum class RunPhase : std::uint8_t {
    idle,
    running,
    passed,
    failed,
};

[[nodiscard]] constexpr std::string_view run_phase_text(
    RunPhase phase) noexcept {
    switch (phase) {
    case RunPhase::idle:
        return "idle";
    case RunPhase::running:
        return "running";
    case RunPhase::passed:
        return "passed";
    case RunPhase::failed:
        return "failed";
    }
    return "idle";
}

struct State final {
    static constexpr std::size_t recipe_capacity = 32U * 1024U;
    static constexpr std::size_t lua_arena_capacity = 96U * 1024U;
    static constexpr std::size_t event_capacity = 32U;
    static constexpr std::size_t event_text_capacity = 1024U;

    struct RunEvent final {
        bool error{};
        std::array<char, event_text_capacity> text{};
        std::size_t size{};
    };

    std::array<std::byte, recipe_capacity> recipe{};
    std::size_t recipe_size{};
    cli::RecipeDigest digest{};
    std::uint64_t next_run{1U};
    std::uint64_t active_run{};
    std::uint64_t last_run{};
    RunPhase run_phase{RunPhase::idle};
    bool active{};

    std::array<std::byte, lua_arena_capacity> lua_arena{};
    std::array<RunEvent, event_capacity> events{};
    std::size_t event_count{};

    std::array<char, event_text_capacity> print_buffer{};
    std::size_t print_size{};
    bool print_truncated{};
    bool event_failure{};

    std::array<char, event_text_capacity + 8U> wire_buffer{};

    [[nodiscard]] bool upload(std::string_view body) {
        if (active || body.empty() || body.size() > recipe.size()) {
            return false;
        }

        for (std::size_t i = 0; i < body.size(); ++i) {
            recipe[i] = static_cast<std::byte>(
                static_cast<unsigned char>(body[i]));
        }

        recipe_size = body.size();
        digest = cli::sha256(
            std::span<std::byte const>{recipe.data(), recipe_size});
        clear_events();
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
        run_phase = RunPhase::running;
        active = true;
        clear_events();
        return true;
    }

    void finish(bool passed) noexcept {
        last_run = active_run;
        run_phase = passed ? RunPhase::passed : RunPhase::failed;
        active = false;
        active_run = 0U;
    }

    [[nodiscard]] std::string_view snapshot_run_state() {
        auto const run = active ? active_run : last_run;
        return wire_state(run, run_phase_text(run_phase));
    }

    [[nodiscard]] bool execute() {
        clear_events();

        apsl::lua::PrintSink print_sink{
            .context = this,
            .write = &State::print_write,
            .finish = &State::print_finish,
        };

        apsl::lua::Runtime runtime{lua_arena, print_sink};
        if (!runtime.valid()) {
            push_event(true, "recipe.lua:1: error: Lua runtime allocation failed");
            return false;
        }

        auto const source = std::string_view{
            reinterpret_cast<char const *>(recipe.data()),
            recipe_size};

        auto status = runtime.load(source, "@recipe.lua");
        if (status != LUA_OK) {
            push_lua_error(runtime.state());
            return false;
        }

        status = runtime.call(0);
        if (status != LUA_OK) {
            push_lua_error(runtime.state());
            return false;
        }

        return !event_failure;
    }

    [[nodiscard]] std::string_view wire_event(std::size_t index) {
        if (index >= event_count) {
            return {};
        }

        auto const &event = events[index];
        auto const prefix = event.error
            ? std::string_view{"err\t"}
            : std::string_view{"out\t"};

        auto const size = std::min(
            wire_buffer.size(),
            prefix.size() + event.size);

        std::memcpy(
            wire_buffer.data(),
            prefix.data(),
            std::min(prefix.size(), size));

        if (size > prefix.size()) {
            std::memcpy(
                wire_buffer.data() + prefix.size(),
                event.text.data(),
                size - prefix.size());
        }

        return {wire_buffer.data(), size};
    }

    [[nodiscard]] std::string_view wire_state(
        std::uint64_t run,
        std::string_view run_state) {
        constexpr std::string_view prefix{"state\t"};
        auto used = std::size_t{};

        auto append = [&](std::string_view part) {
            auto const remaining = wire_buffer.size() - used;
            auto const count = std::min(remaining, part.size());
            if (count > 0U) {
                std::memcpy(
                    wire_buffer.data() + used,
                    part.data(),
                    count);
                used += count;
            }
        };

        append(prefix);

        auto const encoded = std::to_chars(
            wire_buffer.data() + used,
            wire_buffer.data() + wire_buffer.size(),
            run);
        if (encoded.ec != std::errc{}) {
            return {};
        }
        used = static_cast<std::size_t>(
            encoded.ptr - wire_buffer.data());

        append("\t");
        append(run_state);
        return {wire_buffer.data(), used};
    }

private:
    void clear_events() noexcept {
        event_count = 0U;
        print_size = 0U;
        print_truncated = false;
        event_failure = false;
    }

    void push_event(bool error, std::string_view text) noexcept {
        if (event_count >= events.size()) {
            event_failure = true;
            return;
        }

        auto &event = events[event_count++];
        event.error = error;
        event.size = std::min(text.size(), event.text.size());

        if (event.size > 0U) {
            std::memcpy(event.text.data(), text.data(), event.size);
        }
    }

    static void print_write(
        void *context,
        std::string_view text) {
        auto &self = *static_cast<State *>(context);
        auto const remaining =
            self.print_buffer.size() - self.print_size;
        auto const count = std::min(remaining, text.size());

        if (count > 0U) {
            std::memcpy(
                self.print_buffer.data() + self.print_size,
                text.data(),
                count);
            self.print_size += count;
        }

        if (count != text.size()) {
            self.print_truncated = true;
        }
    }

    static void print_finish(void *context) {
        auto &self = *static_cast<State *>(context);
        self.push_event(
            false,
            std::string_view{
                self.print_buffer.data(),
                self.print_size});

        if (self.print_truncated) {
            self.event_failure = true;
            self.push_event(
                true,
                "recipe.lua:1: error: print output exceeded Bison event capacity");
        }

        self.print_size = 0U;
        self.print_truncated = false;
    }

    void push_lua_error(lua_State *lua) noexcept {
        std::size_t size{};
        auto const *raw = lua_tolstring(lua, -1, &size);
        if (raw == nullptr) {
            push_event(
                true,
                "recipe.lua:1: error: unknown Lua error");
            return;
        }

        auto text = std::string_view{raw, size};
        auto const newline = text.find('\n');
        if (newline != std::string_view::npos) {
            text = text.substr(0U, newline);
        }

        constexpr std::string_view prefix{"recipe.lua:"};
        if (!text.starts_with(prefix)) {
            push_event(true, text);
            return;
        }

        auto const message = text.find(':', prefix.size());
        if (message == std::string_view::npos) {
            push_event(true, text);
            return;
        }

        auto const line = text.substr(0U, message);
        auto detail = text.substr(message + 1U);
        while (!detail.empty() && detail.front() == ' ') {
            detail.remove_prefix(1U);
        }

        std::array<char, event_text_capacity> formatted{};
        auto used = std::size_t{};

        auto append = [&](std::string_view part) {
            auto const remaining = formatted.size() - used;
            auto const count = std::min(remaining, part.size());
            if (count > 0U) {
                std::memcpy(
                    formatted.data() + used,
                    part.data(),
                    count);
                used += count;
            }
        };

        append(line);
        append(": error: ");
        append(detail);

        push_event(
            true,
            std::string_view{formatted.data(), used});
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
            state->finish(false);
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
            if (socket.is_open() && state != nullptr) {
                (void)co_await socket.send(
                    state->wire_state(0U, "failed"));
            }
            co_return;
        }

        auto const run = state->active_run;
        if (socket.is_open()) {
            (void)co_await socket.send(
                state->wire_state(run, "running"));
        }

        auto const passed = state->execute();

        for (std::size_t index = 0U;
             index < state->event_count && socket.is_open();
             ++index) {
            auto const message = state->wire_event(index);
            if (!co_await socket.send(message)) {
                state->finish(passed);
                co_return;
            }
        }

        if (socket.is_open()) {
            (void)co_await socket.send(
                state->wire_state(
                    run,
                    passed ? "passed" : "failed"));
        }

        state->finish(passed);
    }
};

struct RunStateSnapshot {
    static constexpr std::string_view Mime_Type{};
    using request_type = std::string_view;

    static std::optional<request_type> parse(std::string_view body) {
        if (!body.empty()) {
            return std::nullopt;
        }
        return body;
    }

    static apsl::web::Response handle(
        apsl::web::Context,
        request_type) {
        if (state == nullptr) {
            return {
                503,
                apsl::web::ContentType::TextPlain,
                "state unavailable\n"};
        }

        return {
            200,
            apsl::web::ContentType::TextPlain,
            state->snapshot_run_state()};
    }
};


constexpr auto router = apsl::web::routes(
    apsl::web::post<"/api/v1/recipes", UploadRecipe>(),
    apsl::web::post<"/api/v1/runs", StartRun>(),
    apsl::web::get<"/api/v1/run/state", RunStateSnapshot>(),
    apsl::web::websocket<"/api/v1/run/events", RunEvents>());

} // namespace bison::server

#endif // BISON_SERVER_HPP_
