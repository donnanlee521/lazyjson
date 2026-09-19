

#include "lazy/lazyjson.hpp"

#include <fstream>
#include <string>

namespace lazy {

//
//
// json class defs

json::char_const_pointer_type json::fastforward_view(
    char_const_pointer_type b, char_const_pointer_type e) noexcept {
  for (; b < e; ++b) {
    if (!std::isspace(*b)) {
      break;
    }
  }
  return b;
}

json::tag_return_type json::tag_json_key(
    json_key_type& tag, char_const_pointer_type _b,
    const char_const_pointer_type e) noexcept {
  auto b{_b};
  bool esc_found{false};
  for (; b < e; ++b) {
    if (*b == '"') {
      const auto i{b - _b};
      if (i > 0 && (*(b - 1) == '\\')) {
        if (i > 1 && (*(b - 2) == '\\')) {
          goto true_return;
        }
      } else {
        goto true_return;
      }
    } else if (*b == '\\') {
      esc_found = true;
    }
  }
  return {b, json_tag_err::invalid_eos};
true_return:
  tag = json_key{std::string_view{_b, static_cast<std::size_t>(b - _b)}};
  return {++b, json_tag_err::success};
}

json::tag_return_type json::tag_json_string(
    string_type& tag, char_const_pointer_type _b,
    const char_const_pointer_type e) noexcept {
  auto b{_b};
  bool escape_found{false};
  for (; b < e; ++b) {
    if (*b == '"') {
      const auto i{b - _b};
      if (i > 0 && (*(b - 1) == '\\')) {
        if (i > 1 && (*(b - 2) == '\\')) {
          goto true_return;
        }
      } else {
        goto true_return;
      }
    } else if (*b == '\\') {
      escape_found = true;
    }
  }
  return {b, json_tag_err::invalid_eos};
true_return:
  tag = escape_found
            ? string_type{std::in_place_index<string_type::esc_inc_tag_idx>, _b,
                          static_cast<std::size_t>(b - _b)}
            : string_type{std::in_place_index<string_type::esc_not_inc_tag_idx>,
                          _b, static_cast<std::size_t>(b - _b)};

  return {++b, json_tag_err::success};
}

json::tag_return_type json::tag_json_value(
    value_type& tags, char_const_pointer_type b,
    const char_const_pointer_type e) noexcept {
  auto [int_tagged, ib] = integer_tag_type::tag(b, e);
  if (!int_tagged) {
    return {ib, json_tag_err::integer_parse_fail};
  }
  auto [flt_tagged, fb] = floating_point_tag_type::tag(b, e, ib - b);
  if (flt_tagged) {
    tags.emplace<floating_type>(std::in_place_type<floating_point_tag_type>,
                                flt_tagged);
    b = fb;
  } else {
    tags.emplace<integer_type>(std::in_place_type<integer_tag_type>,
                               int_tagged);
    b = ib;
  }
  return {b, json_tag_err::success};
}

json::tag_return_type json::tag_json_null(
    value_type& js, char_const_pointer_type b,
    const char_const_pointer_type e) noexcept {
  constexpr static auto N = sizeof(json_null::null_exp) - 1;
  if (!compare_ref_string(b, e, json_null::null_exp)) {
    return {b, json_tag_err::invalid_expression};
  }
  js.emplace<null_type>();
  return {b + N, json_tag_err::success};
}

json::tag_return_type json::tag_json_map(tag_dict_type& dict,
                                         char_const_pointer_type b,
                                         const char_const_pointer_type e) {
  b = {self_type::fastforward_view(b, e)};
  json_tag_err terr{};

  if (b == e) {
    return {b, json_tag_err::invalid_eos};
  }
  if (*b == '}') {
    return {++b, terr};
  }

  // dict.reserve(4);

  while (b < e) {
    // parse key section
    if (*b != '"') {
      terr = json_tag_err::invalid_json_key;
      break;
    }
    ++b;

    json_key_type k;
    std::tie(b, terr) = json::tag_json_key(k, b, e);
    if (terr != json_tag_err{}) {
      break;
    }

    b = self_type::fastforward_view(b, e);
    if (b == e) {
      terr = json_tag_err::invalid_eos;
      break;
    }
    if (*b != ':') {
      terr = json_tag_err::invalid_expression;
      break;
    }
    ++b;

    // parse value section
    auto [iter, is_emplaced] = dict.emplace(std::move(k));
    if (!is_emplaced) {
      terr = json_tag_err::dict_key_duplicate;
      break;
    }

    std::tie(b, terr) = json::tag_json(iter->second, b, e);
    if (terr != json_tag_err{}) {
      break;
    }
    b = self_type::fastforward_view(b, e);
    if (*b == ',') {
      ++b;
    } else if (*b == '}') {
      ++b;
      break;
    } else {
      terr = json_tag_err::invalid_expression;
      break;
    }
    b = self_type::fastforward_view(b, e);
  }
  return {b, terr};
}

json::tag_return_type json::tag_json_array(tag_array_type& arr,
                                           char_const_pointer_type b,
                                           const char_const_pointer_type e) {
  json_tag_err terr{};
  arr.reserve(4);

  b = self_type::fastforward_view(b, e);
  if (b == e) {
    return {b, json_tag_err::invalid_eos};
  }
  if (*b == ']') {
    return {++b, terr};
  }

  while (b < e) {
    std::tie(b, terr) = tag_json(arr.emplace_back(), b, e);
    if (terr != json_tag_err{}) {
      goto out_of_array_parser;
    }

    b = self_type::fastforward_view(b, e);
    if (b == e) {
      // return invalid_eos
      break;
    }
    if (*b == ',') {
      ++b;
    } else if (*b == ']') {
      ++b;
      goto out_of_array_parser;
    }
  }
  return {b, json_tag_err::invalid_eos};
out_of_array_parser:
  return {b, terr};
}

json::tag_return_type json::tag_json(json& json, char_const_pointer_type b,
                                     char_const_pointer_type e) {
  json_tag_err terr{};

  b = fastforward_view(b, e);
  if (b == e) {
    return {b, json_tag_err::invalid_eos};
  }
  switch (*b) {
    case '{': {
      std::tie(b, terr) =
          tag_json_map(json.item.emplace<tag_dict_type>(), ++b, e);
      break;
    }
    case '[': {
      std::tie(b, terr) =
          tag_json_array(json.item.emplace<tag_array_type>(), ++b, e);
      break;
    }
    case '"': {
      std::tie(b, terr) =
          tag_json_string(json.item.emplace<string_type>(), ++b, e);
      break;
    }
    case 't': {
      std::tie(b, terr) = tag_json_boolean<true>(json.item, b, e);
      break;
    }
    case 'f': {
      std::tie(b, terr) = tag_json_boolean<false>(json.item, b, e);
      break;
    }
    case 'n': {
      std::tie(b, terr) = tag_json_null(json.item, b, e);
      break;
    }
    default: {  // value_parse
      std::tie(b, terr) = tag_json_value(json.item, b, e);
      break;
    }
  }
  return {b, terr};
}

json::json(std::string_view val) : item{std::in_place_type<string_type>, val} {}
json::json(tag_array_type&& arr) noexcept
    : item{std::in_place_type<tag_array_type>, std::move(arr)} {}
json::json(tag_dict_type&& dict) noexcept
    : item{std::in_place_type<tag_dict_type>, std::move(dict)} {}

json_array const& json::get_array() const {
  return this->get_container_of<VALUE_ARR_TAG_IDX>();
};
json_dict const& json::get_dict() const {
  return this->get_container_of<VALUE_DICT_TAG_IDX>();
};
json_dict& json::get_dict() {
  return this->get_container_of<VALUE_DICT_TAG_IDX>();
};
json_array& json::get_array() {
  return this->get_container_of<VALUE_ARR_TAG_IDX>();
};

json& json::operator[](std::size_t idx) { return this->get_array().at(idx); }
json const& json::operator[](std::size_t idx) const {
  return this->get_array().at(idx);
}
json& json::operator[](std::basic_string_view<char_type> key) {
  return this->get_dict().at(key);
}
json const& json::operator[](std::basic_string_view<char_type> key) const {
  return this->get_dict().at(key);
}

json::operator bool() const { return std::get<boolean_type>(this->item).get(); }

json& json::at(std::size_t idx) { return this->get_array().at(idx); }
json const& json::at(std::size_t idx) const {
  return this->get_array().at(idx);
}
json& json::at(std::string_view key) { return this->get_dict().at(key); }
json const& json::at(std::string_view key) const {
  return this->get_dict().at(key);
}

json::operator std::string_view() const {
  return std::get<string_type>(this->item).get();
}

std::size_t json::index() const noexcept { return this->item.index(); }

const json& json::parse_all() const {
  std::visit(
      []<class _T>(_T&& v) {
        using T = std::decay_t<_T>;
        if constexpr (std::same_as<T, string_type> ||
                      std::same_as<T, integer_type> ||
                      std::same_as<T, floating_type>) {
          v.convert();
        } else if constexpr (std::same_as<T, tag_array_type>) {
          for (const auto& j : v) {
            j.parse_all();
          }
        } else if constexpr (std::same_as<T, tag_dict_type>) {
          for (const auto& [_, j] : v) {
            j.parse_all();
          }
        } else {
        }
      },
      this->item);
  return *this;
}

std::vector<std::string_view> json::keys() const {
  auto const& dict = this->get_container_of<VALUE_DICT_TAG_IDX>();

  std::vector<std::string_view> rkey{};
  rkey.reserve(dict.size());
  for (auto const& [k, v] : this->get_container_of<VALUE_DICT_TAG_IDX>()) {
    rkey.emplace_back(k);
  }
  return rkey;
}

json json::from_string_view(std::string_view sv, std::size_t* ends) {
  json lz{};
  auto b = sv.begin();
  auto [c, er] = json::tag_json(lz, b, sv.end());

  const auto len{c - b};
  if (er != json_tag_err::success) {
    auto make_err_str = [&](int window_size = 10) -> std::string {
      std::size_t err_stt_idx = std::max(len - window_size, 0l);
      auto err_part = sv.substr(err_stt_idx, window_size * 2);

      auto err_header =
          std::format("json decode error: failed in pos {:d}/{:d} ({:s}), ",
                      len, sv.size(), json_tag_err_to_string(er));
      auto err_body = std::format("...\"{}\"...\n", err_part);
      auto err_tail = std::string(err_header.size() + 3, ' ') +
                      std::string(err_part.size() + 2, '^');

      return err_header + err_body + err_tail;
    };

    throw std::invalid_argument{make_err_str(10)};
  }

  if (ends) {
    *ends = len;
  }
  return lz;
}

//
//
// json_contaienr class defs

json& json_container::get() noexcept { return this->root; }
const json& json_container::get() const noexcept { return this->root; }
json_container::operator bool() const noexcept { return !this->src->empty(); }

json_container json_container::from_file(std::string_view filepath) {
  std::ifstream f{std::string{filepath}};
  if (!f) {
    throw std::invalid_argument{
        std::format("file path({}) not exists", filepath)};
  }
  json_container jc{};
  f >> jc;
  return jc;
}

std::ostream& operator<<(std::ostream& os, json const& self) {
  std::visit(
      [&os](auto&& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::same_as<T, typename json::tag_array_type>) {
          std::size_t i{};
          const std::size_t e{p.size()};
          os << '[';
          for (auto const& v : p) {
            os << v;
            if (++i < e) {
              os << ',';
            }
          }
          os << ']';
        } else if constexpr (std::same_as<T, typename json::tag_dict_type>) {
          std::size_t i{};
          const std::size_t e{p.size()};
          os << '{';
          for (auto const& [k, v] : p) {
            os << k << ":" << v;
            if (++i < e) {
              os << ',';
            }
          }
          os << '}';
        } else {
          os << p;
        }
      },
      self.item);
  return os;
}

std::ostream& operator<<(std::ostream& os, json_container const& self) {
  return os << self.root;
}

std::istream& operator>>(std::istream& is, json_container& self) {
  char c{'\0'};
  if (!(is >> c)) {
    return is;
  }

  char ce = '\0';
  std::string parsed{};
  switch (c) {
    case '{':
      ce = '}';
    case '[': {
      if (ce == '\0') {
        ce = ']';
      }
      parsed = [&is, &c](char cb, char ce) -> std::string {
        std::int16_t brace_count{1};
        std::string s{cb};
        bool str_stt{false};

        while (brace_count > 0 && is.get(c)) {
          if (c == '\\') {
            s.push_back(c);
            if (!is.get(c)) {
              s.clear();
              break;
            }
            s.push_back(c);
            if (c == 'u') {
              char ustr[4];
              if (!is.get(ustr, sizeof ustr)) {
                s.clear();
                break;
              }
              s += ustr;
            }
            continue;
          }

          s.push_back(c);
          if (c == '"') {
            str_stt = !str_stt;
          } else if (!str_stt && c == cb) {
            ++brace_count;
          } else if (!str_stt && c == ce) {
            --brace_count;
          }
        }
        return s;
      }(c, ce);
      break;
    }
    case '"': {
      parsed = [&is, &c]() -> std::string {
        std::string s{c};
        while (is.get(c)) {
          if (c == '\\') {
            s.push_back(c);
            if (!is.get(c)) {
              s.clear();
              break;
            }
            s.push_back(c);
            if (c == 'u') {
              char ustr[4];
              if (!is.get(ustr, sizeof ustr)) {
                s.clear();
                break;
              }
              s += ustr;
            }
            continue;
          }
          s.push_back(c);
        }
        return s;
      }();
      break;
    }
    default: {
      is >> parsed;
      break;
    }
  }
  if (parsed.empty()) {
    return is;
  }
  self = json_container{std::move(parsed)};

  return is;
};

//
//
// helper functions

std::string_view json_tag_err_to_string(json_tag_err val) noexcept {
  constexpr static std::string_view strs[] = {"success",
                                              "invalid_eos",
                                              "invalid_expression",
                                              "invalid_json_key",
                                              "invalid_string_escape_sequence",
                                              "integer_parse_fail",
                                              "dict_key_duplicate"};
  return strs[static_cast<int>(val)];
}

}  // namespace lazy
