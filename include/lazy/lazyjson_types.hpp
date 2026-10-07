
#ifndef LAZYJSON_VALUES_HPP
#define LAZYJSON_VALUES_HPP

#include <cassert>
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
  constexpr json_tag_base(CharT const* data, SizeT size) noexcept;
  constexpr operator bool() const noexcept;

  constexpr static size_type npos = std::numeric_limits<size_type>::max();
};

static_assert(std::is_default_constructible_v<json_tag_base<>>,
              "not default constructible");

struct json_integer_tag : public json_tag_base<char> {
  using char_type = char;

 private:
  using super_type = json_tag_base<char_type>;
  using self_type = json_integer_tag;

 public:
  using size_type = typename super_type::size_type;
  using string_view_type = typename super_type::string_view_type;

  bool is_neg_{};

  constexpr json_integer_tag() noexcept = default;
  constexpr json_integer_tag(char_type const* number_stt_ptr, size_type size,
                             bool is_neg) noexcept;

  static constexpr std::pair<self_type, char_type const*> tag(
      char_type const* b, char_type const* e) noexcept;
  template <std::integral V>
  constexpr parse_result<V> parse() const noexcept;
};

std::ostream& operator<<(std::ostream& os, json_integer_tag const& self);

static_assert(std::is_default_constructible_v<json_integer_tag>);
static_assert(sizeof(json_integer_tag) <= 16, "size is larger then 16");

struct json_float_tag : public json_tag_base<char> {
  using char_type = char;

 private:
  using super_type = json_tag_base<char_type>;
  using self_type = json_float_tag;

 public:
  using size_type = typename super_type::size_type;
  using string_view_type = typename super_type::string_view_type;

  constexpr json_float_tag() noexcept = default;
  constexpr json_float_tag(char_type const* data, size_type len) noexcept;

  static constexpr std::pair<self_type, char_type const*> tag(
      char_type const* _b, char_type const* e, std::size_t fb) noexcept;

  template <std::floating_point V>
  parse_result<V> parse() const noexcept;
};

std::ostream& operator<<(std::ostream& os, json_float_tag const& self);

static_assert(std::is_default_constructible_v<json_float_tag>);
static_assert(sizeof(json_float_tag) <= 24, "size is larger then 24");

template <typename T>
concept lazyjson_type_protocol = requires(T a, std::ostream& os) {
  typename T::value_type;
  { a.get() } -> std::convertible_to<typename T::value_type>;
  { os << a } -> std::convertible_to<std::ostream&>;
};

template <typename T>
concept lazyjson_lazy_type_protocol =
    lazyjson_type_protocol<T> && requires(T a) {
      { a.parse() } -> std::same_as<std::errc>;
    };

struct json_null {
  constexpr static char null_exp[]{"null"};

  using value_type = std::nullptr_t;

 public:
  constexpr value_type get() const noexcept;
  constexpr operator std::string_view() const noexcept;
};

std::ostream& operator<<(std::ostream& os, json_null obj);

static_assert(lazyjson_type_protocol<json_null>);

struct json_boolean {
  constexpr static char true_exp[]{"true"};
  constexpr static char false_exp[]{"false"};

  using value_type = bool;

 private:
  value_type item;

 public:
  constexpr json_boolean() noexcept = default;
  constexpr json_boolean(bool val) noexcept;

  constexpr value_type& get() noexcept;
  constexpr value_type get() const noexcept;

  constexpr operator std::string_view() const noexcept;

  friend std::ostream& operator<<(std::ostream& os, json_boolean obj);
};

std::ostream& operator<<(std::ostream& os, json_boolean obj);

static_assert(lazyjson_type_protocol<json_boolean>);

enum class json_tag_status : uint8_t {
  JSON_TAG_STATUS_TAG = 0,
  JSON_TAG_STATUS_PARSED
};

template <std::integral IntT>
class json_integer {
 public:
  using char_type = char;
  using size_type = std::uint32_t;

  using value_type = IntT;
  using tag_type = json_integer_tag;

 private:
  mutable union {
    tag_type tag_{};
    value_type val_;
  } item_;
  mutable json_tag_status status_{};

 public:
  constexpr json_integer(tag_type const& tag) noexcept;
  constexpr json_integer(value_type val) noexcept;

  std::errc parse() const;
  value_type get() const;

  template <std::integral IntU>
  friend std::ostream& operator<<(std::ostream& os,
                                  json_integer<IntU> const& obj);
};

template <std::integral IntT>
constexpr json_integer<IntT>::json_integer(tag_type const& tag) noexcept
    : item_{.tag_ = tag}, status_{json_tag_status::JSON_TAG_STATUS_TAG} {}

template <std::integral IntT>
constexpr json_integer<IntT>::json_integer(value_type val) noexcept
    : item_{.val_ = val}, status_{json_tag_status::JSON_TAG_STATUS_PARSED} {}

template <std::integral IntT>
std::errc json_integer<IntT>::parse() const {
  if (this->status_ == json_tag_status::JSON_TAG_STATUS_TAG) {
    auto parsed = this->item_.tag_.template parse<IntT>();
    if (parsed.err != std::errc{}) {
      return parsed.err;
    }
    this->item_.val_ = parsed.value;
    this->status_ = json_tag_status::JSON_TAG_STATUS_PARSED;
  }
  return std::errc{};
}

template <std::integral IntT>
json_integer<IntT>::value_type json_integer<IntT>::get() const {
  if (this->parse() != std::errc{}) {
    throw std::invalid_argument{"failed to parse integer string"};
  }
  return this->item_.val_;
}

template <std::integral IntT>
std::ostream& operator<<(std::ostream& os, json_integer<IntT> const& obj) {
  switch (obj.status_) {
    case json_tag_status::JSON_TAG_STATUS_TAG: {
      os << obj.item_.tag_;
      break;
    }
    case json_tag_status::JSON_TAG_STATUS_PARSED: {
      os << obj.item_.val_;
      break;
    }
  }
  return os;
}

static_assert(lazyjson_lazy_type_protocol<json_integer<int>>);
static_assert(sizeof(json_integer<std::size_t>) <= 24);

template <std::floating_point FloatT>
class json_float {
 public:
  using value_type = FloatT;
  using tag_type = json_float_tag;

 private:
  using container_type = std::variant<value_type, tag_type>;
  mutable union {
    tag_type tag_{};
    value_type val_;
  } item_;
  mutable json_tag_status status_;

 public:
  constexpr json_float(tag_type const& tag) noexcept;
  constexpr json_float(value_type val) noexcept;

  std::errc parse() const;
  value_type get() const;

  template <std::floating_point FloatU>
  friend std::ostream& operator<<(std::ostream& os,
                                  json_float<FloatU> const& obj);
};

template <std::floating_point FloatT>
constexpr json_float<FloatT>::json_float(tag_type const& tag) noexcept
    : item_{.tag_ = tag}, status_{json_tag_status::JSON_TAG_STATUS_TAG} {}

template <std::floating_point FloatT>
constexpr json_float<FloatT>::json_float(value_type val) noexcept
    : item_{.val_ = val}, status_{json_tag_status::JSON_TAG_STATUS_PARSED} {}

template <std::floating_point FloatT>
std::errc json_float<FloatT>::parse() const {
  if (this->status_ == json_tag_status::JSON_TAG_STATUS_TAG) {
    auto parsed = this->item_.tag_.template parse<value_type>();
    if (parsed.err != std::errc{}) {
      return parsed.err;
    }
    this->item_.val_ = parsed.value;
  }
  return std::errc{};
}

template <std::floating_point FloatT>
json_float<FloatT>::value_type json_float<FloatT>::get() const {
  if (this->parse() != std::errc{}) {
    throw std::invalid_argument{"failed to parse float string"};
  }
  return this->item_.val_;
}

template <std::floating_point FloatU>
std::ostream& operator<<(std::ostream& os, json_float<FloatU> const& obj) {
  switch (obj.status_) {
    case json_tag_status::JSON_TAG_STATUS_TAG: {
      os << obj.item_.tag_;
      break;
    }
    case json_tag_status::JSON_TAG_STATUS_PARSED: {
      os << obj.item_.val_;
      break;
    }
  }
  return os;
}

static_assert(lazyjson_lazy_type_protocol<json_float<float>>);
static_assert(sizeof(json_float<float>) <= 24);

class json_key {
 public:
  using char_type = char;
  using value_type = std::basic_string_view<char_type>;
  using pointer = char_type*;
  using const_pointer = const char_type*;

  using size_type = std::uint32_t;
  using allocator_type = std::allocator<char_type>;

 private:
  using self_type = json_key;

  size_type size_{};
  bool is_view_{};
  union {
    union {
      pointer ptr_;
      const_pointer cptr_;
    } data_{};
    char_type sso_[16];
  };

  [[no_unique_address]] allocator_type allocator_{};

  pointer allocate_string();
  void destroy_string() noexcept;
  static void copy_string(const_pointer src, size_type len,
                          pointer dst) noexcept;

  inline bool on_sso() const noexcept;
  inline bool on_heap() const noexcept;

  inline pointer get_pointer() noexcept;
  inline const_pointer get_pointer() const noexcept;

 public:
  json_key() noexcept = default;

  // for raw string parser
  template <std::size_t IsView>
  explicit json_key(std::in_place_index_t<IsView> ipi,
                    std::string_view sv) noexcept(IsView > 0);

  // for emplace
  json_key(std::string_view sv);

  json_key(self_type const&);
  self_type& operator=(self_type const&);

  json_key(self_type&&) noexcept;
  self_type& operator=(self_type&&) noexcept;

  ~json_key() noexcept;

  size_type size() const noexcept;
  size_type max_size() const noexcept;
  value_type get() const noexcept;
  operator std::string_view() const noexcept;

  friend std::ostream& operator<<(std::ostream& os, json_key const& obj);
};

bool json_key::on_sso() const noexcept {
  return this->size_ < sizeof(sso_) && !is_view_;
}

bool json_key::on_heap() const noexcept {
  return this->size_ >= sizeof(sso_) && !is_view_;
}

json_key::pointer json_key::get_pointer() noexcept {
  return this->on_sso() ? this->sso_ : this->data_.ptr_;
}

json_key::const_pointer json_key::get_pointer() const noexcept {
  return this->on_sso() ? this->sso_ : this->data_.cptr_;
}

template <std::size_t IsView>
json_key::json_key(std::in_place_index_t<IsView> ipi,
                   std::string_view sv) noexcept(IsView > 0) {
  if constexpr (IsView > 0) {
    this->size_ = sv.size();
    this->is_view_ = (IsView > 0);
    this->data_.cptr_ = sv.data();
  } else {
    auto [s, written_size, err] = lazy::utils::unescape_string(sv);
    if (err != std::errc{}) {
      throw std::invalid_argument{
          "failed to unescape string, invalid escaped string"};
    }
    this->size_ = written_size;
    this->is_view_ = (IsView > 0);
    this->data_.cptr_ = this->allocate_string();
    copy_string(s.data(), s.size(), this->get_pointer());
  }
}

std::strong_ordering operator<=>(json_key const& a, json_key const& b) noexcept;
bool operator==(json_key const& a, json_key const& b) noexcept;
std::strong_ordering operator<=>(json_key const& a,
                                 std::string_view b) noexcept;
bool operator==(json_key const& a, std::string_view b) noexcept;

static_assert(sizeof(json_key) <= 24);
static_assert(std::three_way_comparable<json_key>);

template <typename CharT>
class json_string {
 public:
  using char_type = CharT;
  using value_type = std::basic_string_view<char_type>;
  using parsed_type = std::basic_string<char_type>;
  using tag_type = value_type;

  enum class tag_status : std::uint8_t {
    esc_not_inc_tag = 0,
    esc_inc_tag,
    parsed,
  };

 private:
  using self_type = json_string<CharT>;
  mutable union _u {
    value_type non_escaped_tag_{};
    value_type escaped_tag_;
    parsed_type parsed_;

    constexpr ~_u() noexcept {};
  } item_;

  mutable tag_status index_{};

  template <bool NoCheck = false>
  void delete_if_string(
      tag_status index = tag_status::esc_not_inc_tag) noexcept;

 public:
  constexpr json_string() noexcept = default;
  ~json_string() noexcept;

  json_string(self_type const& other);
  self_type& operator=(json_string const& other);

  json_string(self_type&&) noexcept;
  self_type& operator=(json_string&& other) noexcept;

  template <std::size_t I, typename... Args>
  explicit constexpr json_string(
      std::in_place_index_t<I> ipi,
      Args&&... args) noexcept(I <=
                               static_cast<std::size_t>(
                                   json_string::tag_status::esc_inc_tag));
  explicit constexpr json_string(char_type const* c, std::size_t len);
  constexpr json_string(std::string_view sv);
  constexpr json_string(char_type const* c);

  std::errc parse() const;
  value_type get() const;
  std::size_t index() const noexcept;
  std::size_t size() const noexcept;
  operator value_type() const;

  template <typename CharU>
  friend std::ostream& operator<<(std::ostream& os,
                                  json_string<CharU> const& obj);
};  // class json_string

template <typename CharT>
template <std::size_t I, typename... Args>
constexpr json_string<CharT>::json_string(
    std::in_place_index_t<I> ipi,
    Args&&... args) noexcept(I <=
                             static_cast<std::size_t>(
                                 json_string::tag_status::esc_inc_tag))
    : item_{}, index_{I} {
  if constexpr (I == static_cast<std::size_t>(self_type::tag_status::parsed)) {
    std::construct_at(&item_.parsed_, std::forward<Args>(args)...);
  } else {
    std::construct_at(&item_.non_escaped_tag_, std::forward<Args>(args)...);
  }
}

template <typename CharT>
template <bool NoCheck>
void json_string<CharT>::delete_if_string(tag_status idx) noexcept {
  if constexpr (NoCheck) {
    std::destroy_at(&item_.parsed_);
    this->index_ = idx;
  } else {
    if (this->index_ == self_type::tag_status::parsed) {
      std::destroy_at(&item_.parsed_);
      this->index_ = idx;
    }
  }
}

template <typename CharT>
constexpr json_string<CharT>::json_string(char_type const* c, std::size_t len)
    : json_string(std::in_place_index<static_cast<std::size_t>(
                      json_string::tag_status::parsed)>,
                  c, len) {}

template <typename CharT>
constexpr json_string<CharT>::json_string(std::string_view sv)
    : json_string(std::in_place_index<static_cast<std::size_t>(
                      json_string::tag_status::parsed)>,
                  sv) {}

template <typename CharT>
constexpr json_string<CharT>::json_string(char_type const* c)
    : json_string(std::in_place_index<static_cast<std::size_t>(
                      json_string::tag_status::parsed)>,
                  c) {}

template <typename CharT>
json_string<CharT>::json_string(self_type const& other)
    : item_{}, index_{other.index_} {
  switch (this->index_) {
    case self_type::tag_status::esc_not_inc_tag:
    case self_type::tag_status::esc_inc_tag: {
      std::construct_at(&item_.non_escaped_tag_, other.item_.non_escaped_tag_);
      break;
    }
    case self_type::tag_status::parsed: {
      std::construct_at(&item_.parsed_, other.item_.parsed_);
      break;
    }
  }
}

template <typename CharT>
json_string<CharT>::self_type& json_string<CharT>::operator=(
    self_type const& other) {
  if (this->index_ == other.index_) {
    switch (this->index_) {
      case self_type::tag_status::esc_not_inc_tag:
      case self_type::tag_status::esc_inc_tag: {
        item_.non_escaped_tag_ = other.item_.non_escaped_tag_;
        break;
      }
      case self_type::tag_status::parsed: {
        item_.parsed_ = other.item_.parsed_;
        break;
      }
    }
  } else {
    this->delete_if_string<>(other.index_);
    switch (this->index_) {
      case self_type::tag_status::esc_not_inc_tag:
      case self_type::tag_status::esc_inc_tag: {
        std::construct_at(&item_.non_escaped_tag_,
                          other.item_.non_escaped_tag_);
        break;
      }
      case self_type::tag_status::parsed: {
        std::construct_at(&item_.parsed_, other.item_.parsed_);
        break;
      }
    }
  }
  return *this;
}

template <typename CharT>
json_string<CharT>::json_string(self_type&& other) noexcept
    : item_{}, index_{other.index_} {
  switch (this->index_) {
    case self_type::tag_status::esc_not_inc_tag:
    case self_type::tag_status::esc_inc_tag: {
      std::construct_at(&item_.non_escaped_tag_,
                        std::move(other.item_.non_escaped_tag_));
      break;
    }
    case self_type::tag_status::parsed: {
      std::construct_at(&item_.parsed_, std::move(other.item_.parsed_));
      break;
    }
  }
}

template <typename CharT>
json_string<CharT>::self_type& json_string<CharT>::operator=(
    self_type&& other) noexcept {
  if (this->index_ == other.index_) {
    switch (this->index_) {
      case self_type::tag_status::esc_not_inc_tag:
      case self_type::tag_status::esc_inc_tag: {
        item_.non_escaped_tag_ = std::move(other.item_.non_escaped_tag_);
        break;
      }
      case self_type::tag_status::parsed: {
        item_.parsed_ = std::move(other.item_.parsed_);
        break;
      }
    }
  } else {
    this->delete_if_string<>(other.index_);
    switch (this->index_) {
      case self_type::tag_status::esc_not_inc_tag:
      case self_type::tag_status::esc_inc_tag: {
        std::construct_at(&item_.non_escaped_tag_,
                          std::move(other.item_.non_escaped_tag_));
        break;
      }
      case self_type::tag_status::parsed: {
        std::construct_at(&item_.parsed_, std::move(other.item_.parsed_));
        break;
      }
    }
  }
  return *this;
}

template <typename CharT>
inline json_string<CharT>::~json_string() noexcept {
  this->delete_if_string();
}

template <typename CharT>
std::errc json_string<CharT>::parse() const {
  if (this->index_ == self_type::tag_status::esc_inc_tag) {
    auto&& [unescaped, _, er] =
        lazy::utils::unescape_string(this->item_.escaped_tag_);
    if (er != std::errc{}) {
      return er;
    }
    this->item_.parsed_ = std::move(unescaped);
    this->index_ = self_type::tag_status::parsed;
  }
  return std::errc{};
}

template <typename CharT>
json_string<CharT>::value_type json_string<CharT>::get() const {
  if (this->index_ == self_type::tag_status::esc_not_inc_tag) {
    return this->item_.non_escaped_tag_;
  }
  if (this->parse() != std::errc{}) {
    throw std::invalid_argument{"invalid escaped string"};
  }
  return this->item_.parsed_;
}

template <typename CharT>
std::size_t json_string<CharT>::index() const noexcept {
  return this->index_;
}

template <typename CharT>
std::size_t json_string<CharT>::size() const noexcept {
  std::size_t size{};
  switch (this->index_) {
    case self_type::tag_status::esc_not_inc_tag: {
      size = item_.non_escaped_tag_.size();
      break;
    }
    case self_type::tag_status::esc_inc_tag: {
      size = item_.escaped_tag_.size();
      break;
    }
    case self_type::tag_status::parsed: {
      size = item_.parsed_.size();
      break;
    }
  }
  return size;
}

template <typename CharT>
json_string<CharT>::operator value_type() const {
  return this->get();
}

template <typename CharT>
std::ostream& operator<<(std::ostream& os, json_string<CharT> const& obj) {
  os << '"';
  switch (obj.index_) {
    case json_string<CharT>::tag_status::esc_not_inc_tag: {
      os << obj.item_.non_escaped_tag_;
      break;
    }
    case json_string<CharT>::tag_status::esc_inc_tag: {
      os << obj.item_.escaped_tag_;
      break;
    }
    case json_string<CharT>::tag_status::parsed: {
      os << lazy::utils::escape_string(obj.item_.parsed_);
      break;
    }
  }
  return os << '"';
}

static_assert(lazyjson_lazy_type_protocol<json_string<char>>);
static_assert(sizeof(json_string<char>) <= 32);

namespace experimental {

struct json_float_tag : public json_integer_tag {
 private:
  using super_type = json_integer_tag;
  using self_type = json_float_tag;

 public:
  using char_type = char;
  using value_type = std::basic_string_view<char_type>;
  using size_type = typename super_type::size_type;

  size_type deci_size_;
  size_type fp_stt_;
  size_type fp_size_;
  size_type e_deci_stt_;
  // bool is_neg_;
  bool e_is_neg_;

  constexpr json_float_tag() noexcept = default;

  constexpr json_float_tag(
      const char_type* data, size_type size, bool is_neg, size_type deci_size,
      size_type fstt = std::numeric_limits<std::uint16_t>::max(),
      size_type flen = 0,
      size_type e_deci_stt = std::numeric_limits<std::uint16_t>::max(),
      bool e_is_neg = false) noexcept;

  static constexpr std::pair<std::size_t, self_type> tag(
      value_type s, json_integer_tag const& deci_tag) noexcept;

  template <std::floating_point V>
  constexpr V parse() const noexcept;
};  // class json_float_tag

}  // namespace experimental

}  // namespace lazy

template <>
struct std::hash<lazy::json_key> {
  constexpr std::size_t operator()(lazy::json_key const& k) const noexcept {
    return std::hash<std::string_view>{}(k);
  }
};

#include "lazyjson_types.tpp"
#endif
