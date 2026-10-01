
#include "lazy/lazyjson_value.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>

#include "lazy/lazyjson_utils.hpp"

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

void json_key::init_key(std::string_view view) {
  auto [unescaped, sz, er] = super_type::unescape_if(view);
  if (er != std::errc{}) {
    throw std::invalid_argument{"invalid escaped string"};
  }

  if (unescaped.empty()) {
    this->data_.cptr_ = view.data();
    this->size_ = view.size();
    this->is_view_ = true;
  } else {
    auto new_str =
        std::allocator_traits<allocator_type>::allocate(allocator_, sz + 1);
    *std::uninitialized_copy_n(unescaped.data(), sz, new_str) = '\0';

    this->data_.cptr_ = new_str;
    this->size_ = sz;
    this->is_view_ = false;
  }
}

json_key::char_type* json_key::copy_string(const char_type* src,
                                           size_type src_len) {
  char_type* dst =
      std::allocator_traits<allocator_type>::allocate(allocator_, src_len + 1);
  *std::uninitialized_copy_n(src, src_len, dst) = '\0';
  return dst;
}

void json_key::destroy_string(allocator_type& alloc, char_type* p,
                              size_type len) noexcept {
  std::allocator_traits<allocator_type>::deallocate(alloc, p, len + 1);
}

json_key::json_key(std::string_view sv) { self_type::init_key(sv); }

json_key::json_key(const char_type* c) {
  assert(c != nullptr);
  self_type::init_key(c);
}

json_key::json_key(self_type const& other)
    : data_{.cptr_ = other.is_view_
                         ? other.data_.cptr_
                         : copy_string(other.data_.cptr_, other.size_)},
      size_{other.size_},
      is_view_{other.is_view_} {}

json_key& json_key::operator=(self_type const& other) {
  this->data_.cptr_ = other.is_view_
                          ? other.data_.cptr_
                          : copy_string(other.data_.cptr_, other.size_);
  this->size_ = other.size_;
  this->is_view_ = other.is_view_;
  return *this;
}

json_key::json_key(self_type&& other) noexcept
    : data_{std::exchange(other.data_.cptr_, nullptr)},
      size_{other.size_},
      is_view_(other.is_view_) {}

json_key& json_key::operator=(self_type&& other) noexcept {
  this->data_.cptr_ = std::exchange(other.data_.cptr_, nullptr);
  this->size_ = other.size_;
  this->is_view_ = other.is_view_;
  return *this;
}

json_key::~json_key() noexcept {
  if (this->data_.ptr_ && !this->is_view_) {
    destroy_string(this->allocator_, this->data_.ptr_, this->size_);
  }
}

json_key::value_type json_key::get() const noexcept {
  return value_type{this->data_.cptr_, this->size_};
}

std::size_t json_key::size() const noexcept { return this->size_; }

json_key::operator value_type() const noexcept { return this->get(); }

std::ostream& operator<<(std::ostream& os, json_key const& obj) {
  os << '"';
  if (obj.is_view_) {
    os.write(obj.data_.cptr_, obj.size_);
  } else {
    os << lazy::utils::escape_string(obj);
  }
  os << '"';
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