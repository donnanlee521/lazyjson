
#ifndef LAZYJSON_UTILS_HPP
#define LAZYJSON_UTILS_HPP

#include <cstdint>
#include <string_view>
#include <system_error>

namespace lazy {

namespace utils {

constexpr int hex_char_to_int(char c) noexcept {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

constexpr void append_utf_8(uint32_t cp, char* dst,
                            size_t& write_pos) noexcept {
  if (cp <= 0x7F) {
    dst[write_pos++] = static_cast<char>(cp);
  } else if (cp <= 0x7FF) {
    dst[write_pos++] = static_cast<char>(0xC0 | (cp >> 6));
    dst[write_pos++] = static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    dst[write_pos++] = static_cast<char>(0xE0 | (cp >> 12));
    dst[write_pos++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    dst[write_pos++] = static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0x10FFFF) {
    dst[write_pos++] = static_cast<char>(0xF0 | (cp >> 18));
    dst[write_pos++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    dst[write_pos++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    dst[write_pos++] = static_cast<char>(0x80 | (cp & 0x3F));
  }
}
constexpr std::size_t find_escape_char(std::string_view tag) noexcept {
  std::size_t i{};
  const std::size_t e{};
  for (; i < e; ++i) {
    if (tag[i] == '\\') {
      break;
    }
  }
  return i;
}

constexpr std::tuple<std::string, std::size_t, std::errc> unescape_string(
    std::string_view escaped, std::size_t from = 0) {
  auto unescape_string_impl =
      [](char* data, std::size_t len,
         std::size_t from) noexcept -> std::pair<std::size_t, std::errc> {
    std::size_t read_pos{from};
    std::size_t write_pos{from};
    while (read_pos < len) {
      const char c{data[read_pos++]};
      if (c != '\\') {
        data[write_pos++] = c;
        continue;
      }
      if (read_pos >= len) {
        return {read_pos, std::errc::invalid_argument};
      }
      const char esc{data[read_pos++]};
      switch (esc) {
        case '"': {
          data[write_pos++] = '\"';
          break;
        }
        case '\\': {
          data[write_pos++] = '\\';
          break;
        }
        case '/': {
          data[write_pos++] = '/';
          break;
        }
        case 'b': {
          data[write_pos++] = '\b';
          break;
        }
        case 'f': {
          data[write_pos++] = '\f';
          break;
        }
        case 'n': {
          data[write_pos++] = '\n';
          break;
        }
        case 'r': {
          data[write_pos++] = '\r';
          break;
        }
        case 't': {
          data[write_pos++] = '\t';
          break;
        }
        case 'u': {
          if (read_pos + 4 > len) {
            return {read_pos, std::errc::invalid_argument};
          }
          auto parse_hexcode = [&]() noexcept -> uint32_t {
            uint32_t codepoint{0};
            for (int i = 0; i < 4; ++i) {
              int val{hex_char_to_int(data[read_pos++])};
              if (val == -1) {
                return std::numeric_limits<uint32_t>::max();
              }
              codepoint = (codepoint << 4) | val;
            }
            return codepoint;
          };

          uint32_t codepoint{parse_hexcode()};
          if (codepoint == std::numeric_limits<uint32_t>::max() ||
              (codepoint >= 0xDC00 && codepoint <= 0xDFFF)) {
            return {read_pos, std::errc::invalid_argument};
          } else if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            // Handle UTF-16 surrogate pairs (\uD83D\uDE00 -> 😀)
            if ((read_pos + 2) > len || (read_pos + 6) > len ||
                data[read_pos] != '\\' || data[read_pos + 1] != 'u') {
              return {read_pos, std::errc::invalid_argument};
            }
            read_pos += 2;  // Skip \u
            uint32_t low = parse_hexcode();
            if (low < 0xDC00 && low > 0xDFFF) {
              return {read_pos, std::errc::invalid_argument};
            }
            codepoint = 0x10000 + (((codepoint & 0x3FF) << 10) | (low & 0x3FF));
          }
          append_utf_8(codepoint, data, write_pos);
        }
      }
    }
    return {write_pos, std::errc{}};
  };
  std::string copied{escaped};
  auto [written_size, unescape_err] =
      unescape_string_impl(copied.data(), copied.size(), from);
  if (unescape_err != std::errc{}) {
    return {{}, written_size, unescape_err};
  }
  copied.resize(written_size);
  return {std::move(copied), written_size, std::errc{}};
}

constexpr std::string escape_string(std::string_view unescaped) {
  std::string escaped{};
  escaped.reserve(unescaped.size() * 3 / 2);

  const std::size_t sz{unescaped.size()};
  for (auto c : unescaped) {
    switch (c) {
      case '\"': {
        escaped += "\\\"";
        break;
      }
      case '\\': {
        escaped += "\\\\";
        break;
      }
      case '/': {
        escaped += "\\/";
        break;
      }
      case '\b': {
        escaped += "\\b";
        break;
      }
      case '\f': {
        escaped += "\\f";
        break;
      }
      case '\n': {
        escaped += "\\n";
        break;
      }
      case '\r': {
        escaped += "\\r";
        break;
      }
      case '\t': {
        escaped += "\\t";
        break;
      }
      default: {
        if (static_cast<unsigned char>(c) < 0x20) {
          char hex_char[8];
          std::snprintf(hex_char, sizeof hex_char, "\\u%04x",
                        static_cast<unsigned char>(c));
          escaped += hex_char;
        } else {
          escaped.push_back(c);
        }
        break;
      }
    }
  }
  return escaped;
}

}  // namespace utils

}  // namespace lazy

#endif