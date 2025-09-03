#include "base64.hpp"

#include <array>
#include <stdexcept>

static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Build a decode table that accepts URL-safe alphabet primarily,
// and (optionally) also accepts standard '+' and '/' just data case.
static std::array<int8_t, 256> make_decode_table()
{
    std::array<int8_t, 256> t {};
    t.fill(int8_t { -1 });
    for (int i = 0; i < 64; ++i)
        t[static_cast<unsigned char>(kAlphabet[i])] = static_cast<int8_t>(i);
    t[static_cast<unsigned char>('=')] = int8_t { -2 }; // padding
    // accept classic alphabet too (lenient decoding)
    t[static_cast<unsigned char>('+')] = 62;
    t[static_cast<unsigned char>('/')] = 63;
    // allow ASCII whitespace
    for (unsigned char c : std::string { " \t\r\n" })
        t[c] = int8_t { -3 };
    return t;
}

static const std::array<int8_t, 256> kDecode = make_decode_table();

static bool is_space(unsigned char c)
{
    return kDecode[c] == int8_t { -3 };
}

std::string encode_base64(const std::string& data, bool pad)
{
    const size_t n = data.size();
    if (n == 0)
        return {};

    std::string out;
    out.reserve(((n + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= n) {
        uint32_t x = (uint8_t)data[i] << 16 | (uint8_t)data[i + 1] << 8 | (uint8_t)data[i + 2];
        out.push_back(kAlphabet[(x >> 18) & 0x3F]);
        out.push_back(kAlphabet[(x >> 12) & 0x3F]);
        out.push_back(kAlphabet[(x >> 6) & 0x3F]);
        out.push_back(kAlphabet[x & 0x3F]);
        i += 3;
    }

    const size_t rem = n - i;
    if (rem == 1) {
        uint32_t x = (uint8_t)data[i] << 16;
        out.push_back(kAlphabet[(x >> 18) & 0x3F]);
        out.push_back(kAlphabet[(x >> 12) & 0x3F]);
        if (pad) {
            out.push_back('=');
            out.push_back('=');
        }
    } else if (rem == 2) {
        uint32_t x = ((uint8_t)data[i] << 16) | ((uint8_t)data[i + 1] << 8);
        out.push_back(kAlphabet[(x >> 18) & 0x3F]);
        out.push_back(kAlphabet[(x >> 12) & 0x3F]);
        out.push_back(kAlphabet[(x >> 6) & 0x3F]);
        if (pad)
            out.push_back('=');
    }
    return out;
}

std::string decode_base64(const std::string& data)
{
    if (data.empty())
        return {};

    std::string out;
    out.reserve((data.size() / 4) * 3 + 3);

    int8_t quad[4];
    int q = 0;
    bool seen_pad = false;

    auto flush = [&](int valid) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            int s = quad[i];
            if (s >= 0)
                v = (v << 6) | (uint32_t)s;
            else
                v <<= 6; // padding as zeros
        }
        if (valid >= 2)
            out.push_back(char((v >> 16) & 0xFF));
        if (valid >= 3)
            out.push_back(char((v >> 8) & 0xFF));
        if (valid == 4)
            out.push_back(char(v & 0xFF));
    };

    for (unsigned char c : data) {
        int8_t d = kDecode[c];
        if (d == int8_t { -3 })
            continue; // whitespace
        if (d == int8_t { -1 })
            throw std::invalid_argument("Invalid Base64url character");
        if (seen_pad && !is_space(c))
            throw std::invalid_argument("Data after padding");

        if (d == int8_t { -2 }) { // '='
            if (q < 2)
                throw std::invalid_argument("Misplaced padding");
            quad[q++] = -2;
            seen_pad = true;
            continue;
        }

        quad[q++] = d;
        if (q == 4) {
            int valid = 4;
            if (quad[3] < 0)
                valid = 3;
            if (quad[2] < 0)
                valid = 2;
            if (valid < 2)
                throw std::invalid_argument("Invalid quartet");
            flush(valid);
            q = 0;
        }
    }

    if (q != 0) {
        // Handle no-padding input: length mod 4 can be 2 or 3 (or 0 earlier).
        // q==1 is invalid (at least 2 symbols needed for 1 byte).
        int valid = q;
        if (valid == 1)
            throw std::invalid_argument("Truncated Base64url");
        while (q < 4)
            quad[q++] = -2;
        flush(valid);
    }

    return out;
}
