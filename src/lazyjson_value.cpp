
#include "lazy/lazyjson_value.hpp"

#include <stdexcept>

namespace lazy {

std::ostream& operator<<(std::ostream& os, json_integer_tag const& self) {
  if (self.is_neg_) {
    os << '-';
  }
  return os.write(self.data_, self.size_);
}

std::ostream& operator<<(std::ostream& os, json_float_tag const& self) {
  return os.write(self.data_, self.size_);
}

json_null::value_type json_null::get() const noexcept { return nullptr; }

std::ostream& operator<<(std::ostream& os, json_null obj) {
  return os.write(json_null::null_exp, 4);
}

std::ostream& operator<<(std::ostream& os, json_boolean obj) {
  return os << static_cast<std::string_view>(obj);
}

void json_key::init_key(std::string_view view, element_type& item) {
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

json_key::json_key(std::string_view sv) { self_type::init_key(sv, item); }

json_key::json_key(const char_type* c) {
  assert(c != nullptr);
  self_type::init_key(c, item);
}

json_key::value_type json_key::get() const noexcept {
  return std::visit(
      [](const auto& visited) noexcept -> value_type { return visited; },
      this->item);
}

std::size_t json_key::index() const noexcept { return this->item.index(); }

std::size_t json_key::size() const noexcept {
  return std::visit(
      [](auto const& visited) noexcept -> std::size_t {
        return visited.size();
      },
      this->item);
}

json_key::operator value_type() const noexcept { return this->get(); }

std::ostream& operator<<(std::ostream& os, json_key const& obj) {
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

std::strong_ordering operator<=>(json_key const& a,
                                 json_key const& b) noexcept {
  return a.get() <=> b.get();
}

bool operator==(json_key const& a, json_key const& b) noexcept {
  return a.get() == b.get();
}

std::strong_ordering operator<=>(json_key const& a,
                                 std::string_view b) noexcept {
  return a.get() <=> b;
}

bool operator==(json_key const& a, std::string_view b) noexcept {
  return a.get() == b;
}

std::errc json_string::convert() const {
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

json_string::value_type json_string::get() const {
  if (this->index() == json_string::esc_not_inc_tag_idx) {
    return std::get<json_string::esc_not_inc_tag_idx>(this->item);
  }
  if (this->convert() != std::errc{}) {
    throw std::invalid_argument{"invalid escaped string"};
  }
  return std::get<json_string::parsed_idx>(this->item);
}

std::size_t json_string::index() const noexcept { return this->item.index(); }

std::size_t json_string::size() const noexcept {
  return std::visit(
      [](auto const& visited) noexcept -> std::size_t {
        return visited.size();
      },
      this->item);
}

json_string::operator value_type() const { return this->get(); }

std::ostream& operator<<(std::ostream& os, json_string const& obj) {
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

}  // namespace lazy