#ifndef LAZYJSON_TPP
#define LAZYJSON_TPP

#include <utility>

#include "lazyjson.hpp"

namespace lazy {

template <std::size_t N>
bool json::compare_ref_string(char_const_pointer_type b,
                              char_const_pointer_type e,
                              const char (&cmp)[N]) noexcept {
  constexpr auto N0{N - 1};
  return ((e - b) >= N0) && [&b, &e, &cmp]<std::size_t... Is>(
                                std::index_sequence<Is...>) noexcept {
    return ((cmp[Is] == b[Is]) && ...);
  }(std::make_index_sequence<N0>{});
};

template <typename U, bool GenIfNull>
U& json::get_container_of() {
  if constexpr (GenIfNull) {
    if (std::holds_alternative<null_type>(this->item)) {
      return this->item.emplace<U>();
    }
  }
  return std::get<U>(this->item);
}

template <typename U>
U const& json::get_container_of() const {
  return std::get<U>(this->item);
}

template <bool V>
json::tag_return_type json::tag_json_boolean(
    value_type& tag, char_const_pointer_type b,
    const char_const_pointer_type e) noexcept {
  constexpr static auto N =
      (V ? sizeof(json_boolean::true_exp) : sizeof(json_boolean::false_exp)) -
      1;
  if constexpr (V) {
    if (!compare_ref_string(b, e, json_boolean::true_exp)) {
      return {b, json_tag_err::invalid_expression};
    }
    tag.emplace<json_boolean>(true);
  } else {
    if (!compare_ref_string(b, e, json_boolean::false_exp)) {
      return {b, json_tag_err::invalid_expression};
    }
    tag.emplace<json_boolean>(false);
  }
  return {b + N, json_tag_err::success};
}

template <std::integral T, typename U>
json::json(T val) noexcept : item{std::in_place_type<U>, val} {}

json::json(std::floating_point auto val) noexcept
    : item{std::in_place_type<floating_type>, val} {}

template <std::integral V>
json::operator V() const {
  return std::get<integer_type>(this->item).get();
}
template <std::floating_point V>
json::operator V() const {
  return std::get<floating_type>(this->item).get();
}

template <typename U>
json& json::emplace_back(U&& val) {
  return this->get_container_of<tag_array_type, true>().emplace_back(
      std::forward<U>(val));
}

template <typename U>
decltype(auto) json::emplace(std::basic_string_view<char_type> key, U&& val) {
  return this->get_container_of<tag_dict_type, true>().emplace(
      key, std::forward<U>(val));
}

template <typename T>
bool json::holds() const noexcept {
  return std::holds_alternative<T>(this->item);
}

}  // namespace lazy

#endif
