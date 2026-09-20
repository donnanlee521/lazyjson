
#ifndef LAZYJSON_VALUES_HPP
#define LAZYJSON_VALUES_HPP

#include <cassert>
#include <charconv>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#include "lazyjson_utils.hpp"

namespace lazy {

namespace __impl {

template <std::floating_point R>
constexpr R powof(R base, int exponent) noexcept {
  if (exponent < 0) {
    return R{1.0} / powof<R>(base, -exponent);
  }

  R ret{1.0};
  while (exponent > 0) {
    if (exponent % 2 == 1) {
      ret *= base;
    }
    base *= base;
    exponent /= 2;
  }
  return ret;
}

template <typename CharT>
constexpr bool isdigit(CharT c) noexcept {
  return ('0' <= c) && (c <= '9');
}
}  // namespace __impl

template <typename T>
struct parse_result {
  T value;
  std::errc err;
};

// a lightweight tag object
template <typename CharT = char, typename SizeT = uint16_t>
struct json_tag_base {
  using size_type = SizeT;
  using string_view_type = std::basic_string_view<CharT>;

  CharT const* data_{};
  size_type size_{};

  constexpr json_tag_base() noexcept = default;
  constexpr json_tag_base(CharT const* data, size_type size) noexcept
      : data_{data}, size_{size} {}
  constexpr operator bool() const noexcept {
    return (data_ != nullptr) && (size_ > 0);
  }

  constexpr std::string to_string() const { return std::string{data_, size_}; }

  constexpr static size_type npos = std::numeric_limits<size_type>::max();
};

static_assert(std::is_default_constructible_v<json_tag_base<>>,
              "not default constructible");

template <typename CharT = char>
struct json_integer_tag : public json_tag_base<CharT> {
 private:
  using super_type = json_tag_base<CharT>;
  using self_type = json_integer_tag<CharT>;

 public:
  using size_type = typename super_type::size_type;
  using string_view_type = typename super_type::string_view_type;

  bool is_neg_{};

  constexpr json_integer_tag() noexcept = default;
  constexpr json_integer_tag(CharT const* number_stt_ptr, size_type size,
                             bool is_neg) noexcept
      : super_type{number_stt_ptr, size}, is_neg_{is_neg} {}

  constexpr std::string to_string() const {
    std::string ret(this->size_ + this->is_neg_, '\0');
    auto beg = ret.begin();
    if (this->is_neg_) {
      *beg++ = '-';
    }
    std::uninitialized_copy_n(this->data_, this->size_, beg);
    return ret;
  };

  constexpr static std::pair<self_type, CharT const*> tag(
      CharT const* b, CharT const* e) noexcept {
    if (b >= e) {
      return {{}, e};
    }

    CharT c{*b};
    const bool is_neg{(c == '-')};
    if (is_neg || c == '+') {
      if (++b == e) {
        return {{}, b};
      }
    }
    const auto _b{b};
    for (; b < e; ++b) {
      c = *b;
      if (!__impl::isdigit(c)) {
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
  constexpr parse_result<V> parse() const noexcept {
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
};

static_assert(std::is_default_constructible_v<json_integer_tag<>>);
static_assert(sizeof(json_integer_tag<>) <= 16, "size is larger then 16");

template <typename CharT>
std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os,
                                      json_integer_tag<CharT> const& self) {
  if (self.is_neg_) {
    os << '-';
  }
  return os.write(self.data_, self.size_);
}

template <typename CharT = char>
struct json_float_tag : public json_tag_base<CharT> {
 private:
  using super_type = json_tag_base<CharT>;
  using self_type = json_float_tag<CharT>;

 public:
  using size_type = typename super_type::size_type;
  using string_view_type = typename super_type::string_view_type;

  constexpr json_float_tag() noexcept = default;
  constexpr json_float_tag(CharT const* data, size_type len) noexcept
      : super_type{data, len} {}

  constexpr static std::pair<self_type, CharT const*> tag(
      CharT const* _b, CharT const* e, std::size_t fb) noexcept {
    auto b = _b + fb;

    if (b >= e) {
      return {{}, e};
    }
    CharT c{*b};
    if (c != '.' && c != 'e') {
      // not a float
      return {{}, b};
    }
    if (c == '.') {
      ++b;
      for (; b < e; ++b) {
        c = *b;
        if (!__impl::isdigit(c)) {
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
        if (!__impl::isdigit(c)) {
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
  parse_result<V> parse() const noexcept {
    V parsed;
    auto from_chars_ret =
        std::from_chars(this->data_, this->data_ + this->size_, parsed);

    return {parsed, from_chars_ret.ec};
  }
};

static_assert(std::is_default_constructible_v<json_float_tag<>>);
static_assert(sizeof(json_float_tag<>) <= 24, "size is larger then 24");

template <typename CharT>
std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os,
                                      json_float_tag<CharT> const& self) {
  return os.write(self.data_, self.size_);
}

template <typename T>
struct json_primitive;

template <typename T>
concept lazyjson_type_protocol =
    std::derived_from<T, json_primitive<T>> && requires(T a, std::ostream& os) {
      typename T::value_type;

      { a.get() } -> std::convertible_to<typename T::value_type>;
      { os << a } -> std::convertible_to<std::ostream&>;
    };

template <typename T>
concept lazyjson_lazy_type_protocol =
    lazyjson_type_protocol<T> && requires(T a) {
      { a.convert() } -> std::same_as<std::errc>;
    };

template <typename T>
struct json_primitive {};

struct json_null : public json_primitive<json_null> {
  constexpr static char null_exp[]{"null"};
  using value_type = void;

 private:
  using super_type = json_primitive<json_null>;
  constexpr std::string_view to_string_view_impl() const noexcept {
    return null_exp;
  }

 public:
  inline void get() const noexcept {}
};

inline std::ostream& operator<<(std::ostream& os, json_null obj) {
  return os << obj.null_exp;
}

static_assert(lazyjson_type_protocol<json_null>);

struct json_boolean : public json_primitive<json_boolean> {
  constexpr static char true_exp[]{"true"};
  constexpr static char false_exp[]{"false"};
  using value_type = bool;

 private:
  using super_type = json_primitive<json_boolean>;

  value_type item;

 public:
  constexpr json_boolean() noexcept = default;
  constexpr json_boolean(bool val) noexcept : super_type{}, item{val} {}

  inline constexpr value_type& get() noexcept { return item; }
  inline constexpr value_type get() const noexcept { return item; }

  friend std::ostream& operator<<(std::ostream& os, json_boolean obj);
};

inline std::ostream& operator<<(std::ostream& os, json_boolean obj) {
  return os << (obj.item ? obj.true_exp : obj.false_exp);
}

static_assert(lazyjson_type_protocol<json_boolean>);

template <std::integral IntT, typename CharT = char>
class json_integer : public json_primitive<json_integer<IntT, CharT>> {
 public:
  using value_type = IntT;
  using tag_type = json_integer_tag<CharT>;

 private:
  using container_type = std::variant<value_type, tag_type>;
  mutable container_type item;

 public:
  template <typename U, typename... Args>
  constexpr json_integer(std::in_place_type_t<U> inplace_holder,
                         Args&&... args) noexcept
      : item{inplace_holder, std::forward<Args>(args)...} {}
  constexpr json_integer(value_type val) noexcept
      : item{std::in_place_type<value_type>, val} {}

  constexpr std::size_t index() const noexcept { return this->item.index(); }

  std::errc convert() const {
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

  inline value_type get() const {
    if (this->convert() != std::errc{}) {
      throw std::invalid_argument{"failed to parse integer string"};
    }
    return std::get<value_type>(this->item);
  }

  template <std::integral IntU, typename CharU>
  friend std::basic_ostream<CharU>& operator<<(
      std::basic_ostream<CharU>& os, json_integer<IntU, CharU> const& obj);
};

static_assert(lazyjson_lazy_type_protocol<json_integer<int>>);

template <std::integral IntT, typename CharT>
std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os,
                                      json_integer<IntT, CharT> const& obj) {
  std::visit([&os](auto&& parsed) -> void { os << parsed; }, obj.item);
  return os;
}

template <std::floating_point FloatT, typename CharT = char>
class json_float : public json_primitive<json_float<FloatT, CharT>> {
 public:
  using value_type = FloatT;
  using tag_type = json_float_tag<CharT>;

 private:
  using super_type = json_primitive<json_float<FloatT, CharT>>;
  using container_type = std::variant<value_type, tag_type>;
  mutable container_type item{};

 public:
  template <typename U, typename... Args>
  constexpr json_float(std::in_place_type_t<U> inplace_holder,
                       Args&&... args) noexcept
      : super_type{}, item{inplace_holder, std::forward<Args>(args)...} {}
  constexpr json_float(value_type val) noexcept
      : item{std::in_place_type<value_type>, val} {}

  constexpr std::size_t index() const noexcept { return this->item.index(); }

  std::errc convert() const {
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

  value_type get() const {
    if (this->convert() != std::errc{}) {
      throw std::invalid_argument{"failed to parse float string"};
    }
    return std::get<value_type>(this->item);
  }

  template <std::floating_point FloatU, typename CharU>
  friend std::basic_ostream<CharU>& operator<<(
      std::basic_ostream<CharU>& os, json_float<FloatU, CharU> const& obj);
};

static_assert(lazyjson_lazy_type_protocol<json_float<float>>);

template <std::floating_point FloatU, typename CharU>
std::basic_ostream<CharU>& operator<<(std::basic_ostream<CharU>& os,
                                      json_float<FloatU, CharU> const& obj) {
  std::visit([&os](auto&& parsed) -> void { os << parsed; }, obj.item);
  return os;
}

template <class T>
struct json_string_base : public json_primitive<T> {
 protected:
  static constexpr std::tuple<std::string, std::size_t, std::errc> unescape_if(
      std::string_view s) {
    const auto esc_loc = lazy::utils::find_escape_char(s);
    if (esc_loc == s.size()) {
      // no escape seq found
      return {{}, esc_loc, std::errc{}};
    }
    return lazy::utils::unescape_string(s, esc_loc);
  }

  static constexpr std::tuple<std::string, std::size_t, std::errc> unescape(
      std::string_view s) {
    return lazy::utils::unescape_string(s);
  }
};

class json_key : public json_string_base<json_key> {
 public:
  using char_type = char;
  using value_type = std::basic_string_view<char_type>;
  using parsed_type = std::basic_string<char_type>;
  using tag_type = value_type;
  using element_type = std::variant<tag_type, parsed_type>;

 private:
  using self_type = json_key;
  using super_type = json_string_base<json_key>;

  element_type item;

  static void init_key(std::string_view view, element_type& item) {
    auto [unescaped, _, er] = super_type::unescape_if(view);
    if (er != std::errc{}) {
      throw std::invalid_argument{"invalid escaped string"};
    }
    if (unescaped.empty()) {
      item.emplace<tag_type>(view);
    } else {
      item.emplace<parsed_type>(std::move(unescaped));
    }
  }

 public:
  json_key() noexcept = default;

  json_key(std::string_view sv) { self_type::init_key(sv, item); }

  json_key(const char_type* c) {
    assert(c != nullptr);
    self_type::init_key(c, item);
  }

  value_type get() const noexcept {
    return std::visit(
        [](const auto& visited) noexcept -> value_type { return visited; },
        this->item);
  }
  operator value_type() const noexcept { return this->get(); }

  std::size_t index() const noexcept { return this->item.index(); }

  std::size_t size() const noexcept {
    return std::visit(
        [](auto const& visited) noexcept -> std::size_t {
          return visited.size();
        },
        this->item);
  }

  friend std::ostream& operator<<(std::ostream& os, json_key const& obj);
};

inline std::ostream& operator<<(std::ostream& os, json_key const& obj) {
  std::visit(
      [&os]<class T>(T const& parsed) {
        os << '"';
        if constexpr (std::same_as<T, json_key::tag_type>) {
          os << parsed;
        } else {
          os << lazy::utils::escape_string(parsed);
        }
        os << '"';
      },
      obj.item);
  return os;
}

inline std::strong_ordering operator<=>(json_key const& a,
                                        json_key const& b) noexcept {
  return a.get() <=> b.get();
}

inline bool operator==(json_key const& a, json_key const& b) noexcept {
  return a.get() == b.get();
}

inline std::strong_ordering operator<=>(json_key const& a,
                                        std::string_view b) noexcept {
  return a.get() <=> b;
}

inline bool operator==(json_key const& a, std::string_view b) noexcept {
  return a.get() == b;
}

static_assert(std::three_way_comparable<json_key>);

class json_string : public json_string_base<json_string> {
 public:
  using char_type = char;
  using value_type = std::basic_string_view<char_type>;
  using parsed_type = std::basic_string<char_type>;
  using tag_type = value_type;
  using element_type = std::variant<tag_type,  // tag without escape seq
                                    tag_type,  // tag with escape seq (converted
                                               // to value type when get called)
                                    parsed_type>;

  constexpr static std::size_t esc_not_inc_tag_idx{0};
  constexpr static std::size_t esc_inc_tag_idx{esc_not_inc_tag_idx + 1};
  constexpr static std::size_t parsed_idx{esc_inc_tag_idx + 1};

 private:
  using self_type = json_string;
  using super_type = json_primitive<json_string>;
  mutable element_type item;

 public:
  constexpr json_string() noexcept = default;

  template <std::size_t I, typename... Args>
  constexpr explicit json_string(
      std::in_place_index_t<I> ipi,
      Args&&... args) noexcept(I <= json_string::esc_inc_tag_idx)
      : item{ipi, std::forward<Args>(args)...} {}

  constexpr explicit json_string(char_type const* c, std::size_t len)
      : json_string(std::in_place_index<json_string::parsed_idx>, c, len) {}

  constexpr json_string(std::string_view sv)
      : json_string(std::in_place_index<json_string::parsed_idx>, sv) {}

  constexpr json_string(char_type const* c)
      : json_string(std::in_place_index<json_string::parsed_idx>, c) {}

  std::errc convert() const {
    if (this->item.index() == json_string::esc_inc_tag_idx) {
      auto&& [unescaped, _, er] =
          this->unescape(std::get<json_string::esc_inc_tag_idx>(this->item));
      if (er != std::errc{}) {
        return er;
      }
      this->item.emplace<parsed_type>(std::move(unescaped));
    }
    return std::errc{};
  }

  value_type get() const {
    if (this->index() == json_string::esc_not_inc_tag_idx) {
      return std::get<json_string::esc_not_inc_tag_idx>(this->item);
    }
    if (this->convert() != std::errc{}) {
      throw std::invalid_argument{"invalid escaped string"};
    }
    return std::get<json_string::parsed_idx>(this->item);
  }

  operator value_type() const { return this->get(); }

  std::size_t index() const noexcept { return this->item.index(); }

  friend std::ostream& operator<<(std::ostream& os, json_string const& obj);
};  // class json_string

static_assert(lazyjson_lazy_type_protocol<json_string>);

inline std::ostream& operator<<(std::ostream& os, json_string const& obj) {
  std::visit(
      [&os]<class T>(T const& v) {
        if constexpr (std::same_as<T, json_string::tag_type>) {
          os << '"' << v << '"';
        } else {
          os << '"' << lazy::utils::escape_string(v) << '"';
        }
      },
      obj.item);
  return os;
}

namespace experimental {

template <typename CharT = char>
struct json_float_tag : public json_integer_tag<CharT> {
 private:
  using super_type = json_integer_tag<CharT>;
  using self_type = json_float_tag<CharT>;

 public:
  using value_type = std::basic_string_view<CharT>;
  using size_type = typename super_type::size_type;

  size_type deci_size_;
  size_type fp_stt_;
  size_type fp_size_;
  size_type e_deci_stt_;
  // bool is_neg_;
  bool e_is_neg_;

  constexpr json_float_tag() noexcept = default;

  constexpr json_float_tag(
      const CharT* data, size_type size, bool is_neg, size_type deci_size,
      size_type fstt = std::numeric_limits<std::uint16_t>::max(),
      size_type flen = 0,
      size_type e_deci_stt = std::numeric_limits<std::uint16_t>::max(),
      bool e_is_neg = false) noexcept
      : super_type{data, size, is_neg},
        deci_size_{deci_size},
        fp_stt_{fstt},
        fp_size_{flen},
        e_deci_stt_{e_deci_stt},
        e_is_neg_{e_is_neg} {}

  constexpr static std::pair<std::size_t, self_type> tag(
      value_type s, json_integer_tag<CharT> const& deci_tag) noexcept {
    std::size_t i{};
    const std::size_t e{s.size()};
    const std::size_t b{i};  // 0

    if (i == e) {
      return {0, {}};
    }

    CharT c{s[i]};
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
        if (!__impl::isdigit(c)) {
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
        if (!__impl::isdigit(c)) {
          break;
        }
      }
    }
    std::size_t new_size = deci_tag.size_ + i;
    if (new_size >= super_type::npos || fstt >= super_type::npos ||
        fsize >= super_type::npos || estt >= super_type::npos) {
      return {value_type::npos, {}};
    }
    return {i,
            self_type{deci_tag.data_, static_cast<uint16_t>(new_size),
                      deci_tag.is_neg_, deci_size, static_cast<uint16_t>(fstt),
                      static_cast<uint16_t>(fsize), static_cast<uint16_t>(estt),
                      e_is_neg}};
  }

  template <std::floating_point V>
  constexpr V parse() const noexcept {
    const bool f_exists = (this->fp_stt_ != super_type::npos);
    const bool e_exists = (this->e_deci_stt_ != super_type::npos);

    auto deci =
        json_integer_tag<CharT>{this->data_, this->deci_size_, this->is_neg_}
            .template parse<int64_t, false>();

    uint64_t fnum;
    uint64_t fden;
    if (f_exists) {  // point exists
      fnum = json_integer_tag<CharT>{this->data_ + this->fp_stt_,
                                     this->fp_size_, false}
                 .template parse<uint64_t>(&fden);
    }

    int32_t sci_exp;
    if (e_exists) {
      uint16_t e_deci_size = this->size_ - this->e_deci_stt_;
      sci_exp = json_integer_tag<CharT>{this->data_ + this->e_deci_stt_,
                                        e_deci_size, this->e_is_neg_}
                    .template parse<int32_t>();
    }
    V ret = deci;

    V flt = static_cast<V>(fnum) / fden;
    V exp = __impl::powof<V>(10, sci_exp);

    V rett = (deci + (f_exists ? (static_cast<V>(fnum) / fden) : V{})) *
             (e_exists ? exp : V{1.0});
    return this->is_neg_ ? -rett : rett;
  }
};  // class json_float_tag

}  // namespace experimental

}  // namespace lazy

template <>
struct std::hash<lazy::json_key> {
  constexpr std::size_t operator()(lazy::json_key const& k) noexcept {
    return std::hash<std::string_view>{}(k);
  }
};

#endif