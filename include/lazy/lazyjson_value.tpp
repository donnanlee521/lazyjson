
#ifndef LAZYJSON_VALUE_TPP
#define LAZYJSON_VALUE_TPP

#include <charconv>
// #include <memory>

#include "lazyjson_value.hpp"

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

constexpr std::string json_integer_tag::to_string() const {
  std::string ret(this->size_ + this->is_neg_, '\0');
  auto beg = ret.begin();
  if (this->is_neg_) {
    *beg++ = '-';
  }
  std::uninitialized_copy_n(this->data_, this->size_, beg);
  return ret;
};

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
    if (!utils::isdigit(c)) {
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
      if (!utils::isdigit(c)) {
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
      if (!utils::isdigit(c)) {
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

constexpr json_boolean::json_boolean(bool val) noexcept
    : super_type{}, item{val} {}

constexpr json_boolean::value_type& json_boolean::get() noexcept {
  return item;
}

constexpr json_boolean::value_type json_boolean::get() const noexcept {
  return item;
}

constexpr json_boolean::operator std::string_view() const noexcept {
  return this->item ? true_exp : false_exp;
}

template <std::integral IntT>
template <typename U, typename... Args>
constexpr json_integer<IntT>::json_integer(
    std::in_place_type_t<U> inplace_holder, Args&&... args) noexcept
    : super_type{}, item{inplace_holder, std::forward<Args>(args)...} {}

template <std::integral IntT>
constexpr json_integer<IntT>::json_integer(value_type val) noexcept
    : super_type{}, item{std::in_place_type<value_type>, val} {}

template <std::integral IntT>
constexpr std::size_t json_integer<IntT>::index() const noexcept {
  return this->item.index();
}

template <std::integral IntT>
std::errc json_integer<IntT>::convert() const {
  if (std::holds_alternative<tag_type>(this->item)) {
    auto const& tagged = std::get<tag_type>(this->item);
    auto parsed = tagged.template parse<value_type>();
    if (parsed.err != std::errc{}) {
      return parsed.err;
    }
    item.template emplace<value_type>(parsed.value);
  }
  return std::errc{};
}

template <std::integral IntT>
json_integer<IntT>::value_type json_integer<IntT>::get() const {
  if (this->convert() != std::errc{}) {
    throw std::invalid_argument{"failed to parse integer string"};
  }
  return std::get<value_type>(this->item);
}

template <std::integral IntT>
std::ostream& operator<<(std::ostream& os, json_integer<IntT> const& obj) {
  std::visit([&os](auto&& parsed) -> void { os << parsed; }, obj.item);
  return os;
}

template <std::floating_point FloatT>
template <typename U, typename... Args>
constexpr json_float<FloatT>::json_float(std::in_place_type_t<U> inplace_holder,
                                         Args&&... args) noexcept
    : super_type{}, item{inplace_holder, std::forward<Args>(args)...} {}

template <std::floating_point FloatT>
constexpr json_float<FloatT>::json_float(value_type val) noexcept
    : super_type{}, item{std::in_place_type<value_type>, val} {}

template <std::floating_point FloatT>
constexpr std::size_t json_float<FloatT>::index() const noexcept {
  return this->item.index();
}

template <std::floating_point FloatT>
std::errc json_float<FloatT>::convert() const {
  if (std::holds_alternative<tag_type>(item)) {
    auto const& tagged = std::get<tag_type>(item);
    auto parsed = tagged.template parse<value_type>();
    if (parsed.err != std::errc{}) {
      return parsed.err;
    }
    this->item.template emplace<value_type>(parsed.value);
  }
  return std::errc{};
}

template <std::floating_point FloatT>
json_float<FloatT>::value_type json_float<FloatT>::get() const {
  if (this->convert() != std::errc{}) {
    throw std::invalid_argument{"failed to parse float string"};
  }
  return std::get<value_type>(this->item);
}

template <std::floating_point FloatU>
std::ostream& operator<<(std::ostream& os, json_float<FloatU> const& obj) {
  std::visit([&os](auto&& parsed) -> void { os << parsed; }, obj.item);
  return os;
}

template <class T>
constexpr std::tuple<std::string, std::size_t, std::errc>
json_string_base<T>::unescape_if(std::string_view s) {
  const auto esc_loc = lazy::utils::find_escape_char(s);
  if (esc_loc == s.size()) {
    // no escape seq found
    return {{}, esc_loc, std::errc{}};
  }
  return lazy::utils::unescape_string(s, esc_loc);
}

template <class T>
constexpr std::tuple<std::string, std::size_t, std::errc>
json_string_base<T>::unescape(std::string_view s) {
  return lazy::utils::unescape_string(s);
}

template <std::size_t I, typename... Args>
constexpr json_string::json_string(
    std::in_place_index_t<I> ipi,
    Args&&... args) noexcept(I <= json_string::esc_inc_tag_idx)
    : item{ipi, std::forward<Args>(args)...} {}

constexpr json_string::json_string(char_type const* c, std::size_t len)
    : json_string(std::in_place_index<json_string::parsed_idx>, c, len) {}

constexpr json_string::json_string(std::string_view sv)
    : json_string(std::in_place_index<json_string::parsed_idx>, sv) {}

constexpr json_string::json_string(char_type const* c)
    : json_string(std::in_place_index<json_string::parsed_idx>, c) {}

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
      if (!utils::isdigit(c)) {
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
      if (!utils::isdigit(c)) {
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