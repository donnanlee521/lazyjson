
#ifndef LAZYJSON_VALUE_TPP
#define LAZYJSON_VALUE_TPP

#include <charconv>
#include <memory>

#include "lazyjson_types.hpp"

namespace lazy {

template <typename CharT, typename SizeT>
constexpr json_tag_base<CharT, SizeT>::json_tag_base(CharT const* data,
                                                     SizeT size) noexcept
    : data_{data}, size_{size} {}

template <typename CharT, typename SizeT>
constexpr json_tag_base<CharT, SizeT>::operator bool() const noexcept {
  return (data_ != nullptr) && (size_ > 0);
}

constexpr json_integer_tag::json_integer_tag(char_type const* number_stt_ptr,
                                             size_type size,
                                             bool is_neg) noexcept
    : super_type{number_stt_ptr, size}, is_neg_{is_neg} {}

constexpr std::pair<json_integer_tag::self_type,
                    json_integer_tag::char_type const*>
json_integer_tag::tag(char_type const* b, char_type const* e) noexcept {
  if (b >= e) {
    return {{}, e};
  }

  char_type c{*b};
  const bool is_neg{(c == '-')};
  if (is_neg || c == '+') {
    if (++b == e) {
      return {{}, b};
    }
  }
  const auto _b{b};
  for (; b < e; ++b) {
    c = *b;
    if (!::isdigit(c)) {
      break;
    }
  }
  const std::size_t digit_len = b - _b;
  if (digit_len >= super_type::npos) {
    return {{}, b};
  }
  return {self_type{_b, static_cast<size_type>(digit_len), is_neg}, b};
}

template <std::integral V>
constexpr parse_result<V> json_integer_tag::parse() const noexcept {
  V val{};
  for (uint16_t i{}; i < this->size_; ++i) {
    V digit = this->data_[i] - '0';
    if (val > (std::numeric_limits<V>::max() - digit + this->is_neg_) / 10) {
      return {val, std::errc::result_out_of_range};
    }
    val = 10 * val + digit;
  }
  if (this->is_neg_) {
    val = -val;
  }
  return parse_result<V>{val, std::errc{}};
}

constexpr json_float_tag::json_float_tag(char_type const* data,
                                         size_type len) noexcept
    : super_type{data, len} {}

constexpr std::pair<json_float_tag, json_float_tag::char_type const*>
json_float_tag::tag(char_type const* _b, char_type const* e,
                    std::size_t fb) noexcept {
  auto b = _b + fb;

  if (b >= e) {
    return {{}, e};
  }
  char_type c{*b};
  if (c != '.' && c != 'e') {
    // not a float
    return {{}, b};
  }
  if (c == '.') {
    ++b;
    for (; b < e; ++b) {
      c = *b;
      if (!::isdigit(c)) {
        break;
      }
    }
  }
  if (c == 'e') {
    if (++b == e) {
      return {{}, b};
    }
    c = *b;
    bool e_is_neg = (c == '-');
    if (e_is_neg || c == '+') {
      if (++b == e) {
        return {{}, b};
      }
    }
    for (; b < e; ++b) {
      c = *b;
      if (!::isdigit(c)) {
        break;
      }
    }
  }
  std::size_t len = b - _b;
  if (len >= super_type::npos) {
    return {{}, b};
  }
  return {self_type{_b, static_cast<size_type>(len)}, b};
}

template <std::floating_point V>
parse_result<V> json_float_tag::parse() const noexcept {
  V parsed;
  auto from_chars_ret =
      std::from_chars(this->data_, this->data_ + this->size_, parsed);

  return {parsed, from_chars_ret.ec};
}

constexpr json_null::operator std::string_view() const noexcept {
  return json_null::null_exp;
}

constexpr json_null::value_type json_null::get() const noexcept {
  return nullptr;
}

constexpr json_boolean::json_boolean(bool val) noexcept : item{val} {}

constexpr json_boolean::value_type& json_boolean::get() noexcept {
  return item;
}

constexpr json_boolean::value_type json_boolean::get() const noexcept {
  return item;
}

constexpr json_boolean::operator std::string_view() const noexcept {
  return this->item ? true_exp : false_exp;
}

namespace experimental {

constexpr json_float_tag::json_float_tag(const char_type* data, size_type size,
                                         bool is_neg, size_type deci_size,
                                         size_type fstt, size_type flen,
                                         size_type e_deci_stt,
                                         bool e_is_neg) noexcept
    : super_type{data, size, is_neg},
      deci_size_{deci_size},
      fp_stt_{fstt},
      fp_size_{flen},
      e_deci_stt_{e_deci_stt},
      e_is_neg_{e_is_neg} {}

constexpr std::pair<std::size_t, json_float_tag> json_float_tag::tag(
    value_type s, json_integer_tag const& deci_tag) noexcept {
  std::size_t i{};
  const std::size_t e{s.size()};
  const std::size_t b{i};  // 0

  if (i == e) {
    return {0, {}};
  }

  char_type c{s[i]};
  if (c != '.' && c != 'e') {
    return {0, {}};
  }
  auto deci_size = deci_tag.size_;

  std::size_t fstt{super_type::npos};
  std::size_t fsize{0};
  if (c == '.') {
    fstt = ++i + deci_size;
    for (; i < e; ++i) {
      c = s[i];
      if (!::isdigit(c)) {
        break;
      }
    }
    fsize = i - (fstt - deci_size);
  }

  std::size_t estt{super_type::npos};
  bool e_is_neg{false};
  if (c == 'e') {
    if (++i == e) {
      return {value_type::npos, {}};
    }
    c = s[i];
    e_is_neg = (c == '-');
    if (e_is_neg || c == '+') {
      if (++i == e) {
        return {value_type::npos, {}};
      }
    }
    estt = i + deci_size;
    for (; i < e; ++i) {
      c = s[i];
      if (!::isdigit(c)) {
        break;
      }
    }
  }
  std::size_t new_size = deci_tag.size_ + i;
  if (new_size >= super_type::npos || fstt >= super_type::npos ||
      fsize >= super_type::npos || estt >= super_type::npos) {
    return {value_type::npos, {}};
  }
  return {i, self_type{deci_tag.data_, static_cast<uint16_t>(new_size),
                       deci_tag.is_neg_, deci_size, static_cast<uint16_t>(fstt),
                       static_cast<uint16_t>(fsize),
                       static_cast<uint16_t>(estt), e_is_neg}};
}

template <std::floating_point V>
constexpr V json_float_tag::parse() const noexcept {}

}  // namespace experimental

}  // namespace lazy

#endif