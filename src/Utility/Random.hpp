#pragma once

#include <string>
#include <random>

namespace chess::util {

inline std::string RandomAlphaString(const std::size_t length) {
    constexpr std::string_view characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd{};
    std::mt19937 generator(rd());

    std::uniform_int_distribution<std::size_t> distribution(0, characters.size() - 1);

    std::string random_string{};
    random_string.reserve(length);

    for (std::size_t it = 0; it < length; it++) {
        random_string += characters[distribution(generator)];
    }

    return random_string;
}

} // namespace chess::util
