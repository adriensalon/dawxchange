#pragma once

#include <string>

[[nodiscard]] std::string encode_base64(const std::string& data, bool pad = false);
[[nodiscard]] std::string decode_base64(const std::string& data);
