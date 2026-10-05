#include <bison/run_client.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace bison::cli {

namespace {

[[nodiscard]] constexpr std::uint32_t rotr(
    std::uint32_t value,
    unsigned shift) noexcept {
    return std::rotr(value, static_cast<int>(shift));
}

constexpr std::array<std::uint32_t, 64U> k{
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

void transform(
    std::array<std::uint32_t, 8U> &state,
    std::byte const *block) noexcept {
    std::array<std::uint32_t, 64U> w{};

    for (std::size_t i = 0; i < 16U; ++i) {
        auto const j = i * 4U;
        w[i] =
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(block[j])) << 24U) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(block[j + 1U])) << 16U) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(block[j + 2U])) << 8U) |
            static_cast<std::uint32_t>(std::to_integer<unsigned char>(block[j + 3U]));
    }

    for (std::size_t i = 16U; i < 64U; ++i) {
        auto const s0 = rotr(w[i - 15U], 7U) ^ rotr(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
        auto const s1 = rotr(w[i - 2U], 17U) ^ rotr(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    auto a = state[0]; auto b = state[1]; auto c = state[2]; auto d = state[3];
    auto e = state[4]; auto f = state[5]; auto g = state[6]; auto h = state[7];

    for (std::size_t i = 0; i < 64U; ++i) {
        auto const s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        auto const ch = (e & f) ^ ((~e) & g);
        auto const temp1 = h + s1 + ch + k[i] + w[i];
        auto const s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        auto const maj = (a & b) ^ (a & c) ^ (b & c);
        auto const temp2 = s0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

[[nodiscard]] std::string recipe_name(std::string_view path) {
    auto const slash = path.find_last_of("/\\");
    return std::string{
        slash == std::string_view::npos
            ? path
            : path.substr(slash + 1U)};
}

} // namespace

RecipeDigest sha256(std::span<std::byte const> bytes) noexcept {
    std::array<std::uint32_t, 8U> state{
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};

    auto offset = std::size_t{};
    while (bytes.size() - offset >= 64U) {
        transform(state, bytes.data() + offset);
        offset += 64U;
    }

    std::array<std::byte, 128U> tail{};
    auto const remaining = bytes.size() - offset;
    for (std::size_t i = 0; i < remaining; ++i) {
        tail[i] = bytes[offset + i];
    }
    tail[remaining] = std::byte{0x80};

    auto const total_bits = static_cast<std::uint64_t>(bytes.size()) * 8U;
    auto const blocks = remaining < 56U ? 1U : 2U;
    auto const length_offset = blocks * 64U - 8U;
    for (std::size_t i = 0; i < 8U; ++i) {
        tail[length_offset + i] =
            static_cast<std::byte>((total_bits >> ((7U - i) * 8U)) & 0xffU);
    }

    for (std::size_t i = 0; i < blocks; ++i) {
        transform(state, tail.data() + i * 64U);
    }

    RecipeDigest result{};
    for (std::size_t i = 0; i < state.size(); ++i) {
        result.bytes[i * 4U] = static_cast<std::uint8_t>(state[i] >> 24U);
        result.bytes[i * 4U + 1U] = static_cast<std::uint8_t>(state[i] >> 16U);
        result.bytes[i * 4U + 2U] = static_cast<std::uint8_t>(state[i] >> 8U);
        result.bytes[i * 4U + 3U] = static_cast<std::uint8_t>(state[i]);
    }
    return result;
}

std::array<char, 64U> hex(RecipeDigest const &digest) noexcept {
    constexpr std::string_view digits{"0123456789abcdef"};
    std::array<char, 64U> result{};

    for (std::size_t i = 0; i < digest.bytes.size(); ++i) {
        auto const value = digest.bytes[i];
        result[i * 2U] = digits[(value >> 4U) & 0x0fU];
        result[i * 2U + 1U] = digits[value & 0x0fU];
    }
    return result;
}

RunResult RunClient::run(
    std::string_view host,
    std::string_view recipe_path,
    EventSink &sink) {
    std::ifstream input{std::string{recipe_path}, std::ios::binary};
    if (!input) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: cannot read recipe",
        });
        return RunResult::transport_error;
    }

    std::vector<char> storage{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};

    if (storage.empty()) {
        sink.emit({
            .stream = Stream::err,
            .text = "bison: recipe is empty",
        });
        return RunResult::failed;
    }

    auto const recipe = std::span<std::byte const>{
        reinterpret_cast<std::byte const *>(storage.data()),
        storage.size()};
    auto const digest = sha256(recipe);
    auto const name = recipe_name(recipe_path);

    if (!transport_.upload_recipe(
            host,
            name,
            recipe,
            digest,
            sink)) {
        return RunResult::transport_error;
    }

    RunHandle run{};
    if (!transport_.start_run(
            host,
            digest,
            run,
            sink)) {
        return RunResult::transport_error;
    }

    return transport_.stream_run(
        host,
        run,
        sink);
}

} // namespace bison::cli
