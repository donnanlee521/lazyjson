
#ifndef LAZYJSON_HPP
#define LAZYJSON_HPP

#include <array>
#include <cctype>
#include <concepts>
#include <cstdint>
#include <functional>
#include <iostream>
#include <istream>
#include <memory>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "lazyjson_concept.hpp"
#include "lazyjson_containers.hpp"
#include "lazyjson_value.hpp"

namespace lazy {

enum class json_tag_err {
  success = 0,
  invalid_eos,
  invalid_expression,
  invalid_json_key,
  invalid_string_escape_sequence,
  integer_parse_fail,
  dict_key_duplicate,
};

std::string_view json_tag_err_to_string(json_tag_err val) noexcept;

class json;
template <std::size_t I>
class json_formatter;

using json_key_type = json_key;
using json_array = std::vector<json>;
using json_dict = json_ordered_dict<json_key_type, json>;

class json {
  using self_type = json;

 public:
  using char_type = char;
  using char_pointer_type = char_type*;
  using char_const_pointer_type = const char_type*;

  using null_type = json_null;
  using boolean_type = json_boolean;
  using string_type = json_string;
  using integer_type = json_integer<int64_t>;
  using floating_type = json_float<double>;

  using integer_tag_type = integer_type::tag_type;
  using floating_point_tag_type = floating_type::tag_type;

  using tag_array_type = json_array;
  using tag_dict_type = json_dict;

  using value_type =
      std::variant<null_type, boolean_type, string_type, integer_type,
                   floating_type, tag_array_type, tag_dict_type>;

  constexpr static std::size_t VALUE_ARR_TAG_IDX{5};
  constexpr static std::size_t VALUE_DICT_TAG_IDX{6};

 private:
  value_type item;

  static char_const_pointer_type fastforward_view(
      char_const_pointer_type b, char_const_pointer_type e) noexcept;
  template <std::size_t N>
  static bool compare_ref_string(char_const_pointer_type b,
                                 char_const_pointer_type e,
                                 const char (&cmp)[N]) noexcept;

  template <typename U, bool GenIfNull = false>
  U& get_container_of();
  template <typename U>
  U const& get_container_of() const;

  using tag_return_type = std::pair<char_const_pointer_type, json_tag_err>;

  static tag_return_type tag_json_key(
      json_key_type& tag, char_const_pointer_type beg,
      const char_const_pointer_type end) noexcept;
  static tag_return_type tag_json_string(
      string_type& tag, char_const_pointer_type beg,
      const char_const_pointer_type end) noexcept;
  static tag_return_type tag_json_value(
      value_type& tag, char_const_pointer_type beg,
      const char_const_pointer_type end) noexcept;

  template <bool V>
  static tag_return_type tag_json_boolean(
      value_type& tag, char_const_pointer_type beg,
      const char_const_pointer_type end) noexcept;
  static tag_return_type tag_json_null(
      value_type& tag, char_const_pointer_type beg,
      const char_const_pointer_type end) noexcept;

  static tag_return_type tag_json_map(value_type& tag,
                                      char_const_pointer_type beg,
                                      const char_const_pointer_type end);
  static tag_return_type tag_json_array(tag_array_type& arr,
                                        char_const_pointer_type beg,
                                        const char_const_pointer_type end);

  static tag_return_type tag_json(json& json, char_const_pointer_type beg,
                                  const char_const_pointer_type end);

 public:
  json() noexcept = default;

  template <std::integral T,
            typename U = std::conditional_t<std::is_same_v<T, bool>,
                                            boolean_type, integer_type>>
  json(T val) noexcept;
  json(std::floating_point auto val) noexcept;
  json(std::string_view val);
  json(tag_array_type&& arr) noexcept;
  json(tag_dict_type&& dict) noexcept;

  template <std::integral V>
  operator V() const;
  template <std::floating_point V>
  operator V() const;
  operator bool() const;
  operator std::string_view() const;
  operator std::nullptr_t() const;

  json_array& get_array();
  json_array const& get_array() const;
  json_dict& get_dict();
  json_dict const& get_dict() const;

  std::size_t index() const noexcept;

  template <typename U>
  json& emplace_back(U&& val);
  template <typename U>
  decltype(auto) emplace(std::basic_string_view<char_type> key, U&& val);

  template <typename T>
  bool holds() const noexcept;

  json& operator[](std::size_t idx);
  json const& operator[](std::size_t idx) const;
  json& operator[](std::basic_string_view<char_type> key);
  json const& operator[](std::basic_string_view<char_type> key) const;

  json& at(std::size_t idx);
  json const& at(std::size_t idx) const;
  json& at(std::string_view idx);
  json const& at(std::string_view idx) const;

  json const& parse_all() const;
  std::vector<std::string_view> keys() const;

  static json from_string_view(std::string_view json_raw,
                               std::size_t* e = nullptr);

  friend std::ostream& operator<<(std::ostream& os, json const& self);

  template <std::size_t J>
  friend std::ostream& operator<<(std::ostream& os,
                                  json_formatter<J> const& val);
};

/// @brief json wrapper class for safe string lifetime garantee
class json_container {
  using self_type = json_container;

  // because of sso, make string on heap
  std::unique_ptr<std::string> src;
  json root;

 public:
  json_container() noexcept = default;

  json_container(string_view_convertible<char> auto&& raw_json_str)
      : src{std::make_unique<std::string>(
            std::forward<decltype(raw_json_str)>(raw_json_str))},
        root{json::from_string_view(*src)} {}

  static json_container from_file(std::string_view filepath);

  json& get() noexcept;
  json const& get() const noexcept;

  operator bool() const noexcept;

  friend std::ostream& operator<<(std::ostream& os, json_container const& self);
  friend std::istream& operator>>(std::istream& is, json_container& self);
};

template <std::size_t I = 4>
class json_formatter {
  std::reference_wrapper<const json> ref_;
  std::uint32_t lvl_;

  consteval static std::array<char, I> make_indent() noexcept {
    std::array<char, I> indent;
    indent.fill(' ');
    return indent;
  }

  constexpr static std::array<char, I> indent{make_indent()};

  static std::ostream& write_indent(std::ostream& os, std::size_t lvl) {
    for (std::size_t i{}; i < lvl; ++i) {
      os.write(indent.data(), I);
    }
    return os;
  }

 public:
  json_formatter(json const& ref, std::uint32_t lvl = 0) noexcept
      : ref_{ref}, lvl_{lvl} {}

  template <std::size_t J>
  friend std::ostream& operator<<(std::ostream& os,
                                  json_formatter<J> const& val);
};

template <std::size_t J>
std::ostream& operator<<(std::ostream& os, json_formatter<J> const& val) {
  std::visit(
      [&os, &val](auto&& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::same_as<T, typename json::tag_array_type>) {
          std::size_t i{};
          const std::size_t e{p.size()};
          val.write_indent(os << "[\n", val.lvl_ + 1);
          for (auto const& v : p) {
            os << json_formatter<J>{v, val.lvl_ + 1};
            if (++i < e) {
              val.write_indent(os << ",\n", val.lvl_ + 1);
            }
          }
          val.write_indent(os << '\n', val.lvl_) << ']';
          // val.write_indent(os, val.lvl_);
        } else if constexpr (std::same_as<T, typename json::tag_dict_type>) {
          std::size_t i{};
          const std::size_t e{p.size()};

          val.write_indent(os << "{\n", val.lvl_ + 1);
          for (auto const& [k, v] : p) {
            os << k << " : " << json_formatter<J>{v, val.lvl_ + 1};
            if (++i < e) {
              val.write_indent(os << ",\n", val.lvl_ + 1);
            }
          }
          val.write_indent(os << '\n', val.lvl_) << '}';
        } else {
          os << p;
        }
      },
      val.ref_.get().item);
  return os;
}

}  // namespace lazy

#include "lazyjson.tpp"
#endif
