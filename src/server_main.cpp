#include <bison/server.hpp>

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <string_view>

int main(int argc, char **argv) {
    auto port = std::uint16_t{8080U};

    if (argc > 2) {
        std::fprintf(stderr, "usage: bison-server [PORT]\n");
        return 2;
    }

    if (argc == 2) {
        unsigned parsed{};
        auto const input = std::string_view{argv[1]};
        auto const result = std::from_chars(
            input.data(),
            input.data() + input.size(),
            parsed);

        if (result.ec != std::errc{} ||
            result.ptr != input.data() + input.size() ||
            parsed == 0U ||
            parsed > 65535U) {
            std::fprintf(stderr, "bison-server: invalid port\n");
            return 2;
        }

        port = static_cast<std::uint16_t>(parsed);
    }

    bison::server::State state{};
    bison::server::state = &state;

    apsl::web::Server<
        decltype(bison::server::router),
        4U,
        bison::server::State::recipe_capacity + 4096U>
        server{bison::server::router};

    std::printf("bison-server: http://127.0.0.1:%u\n", port);
    return server.run(port);
}
