
#include "lazy/lazyjson_types.hpp"

#include <cstddef>
#include <limits>
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

std::ostream& operator<<(std::ostream& os, json_null obj) {
  return os.write(json_null::null_exp, 4);
}

std::ostream& operator<<(std::ostream& os, json_boolean obj) {
  return os << static_cast<std::string_view>(obj);
}

json_key::pointer json_key::allocate_string() {
  return this->on_sso() ? nullptr
                        : std::allocator_traits<allocator_type>::allocate(
                              allocator_, this->size_ + 1);
}

void json_key::copy_string(const_pointer src, size_type len,
                           pointer dst) noexcept {
  *std::uninitialized_copy_n(src, len, dst) = '\0';
}

void json_key::destroy_string() noexcept {
  if (this->data_.cptr_ && this->on_heap()) {
    std::allocator_traits<allocator_type>::deallocate(
        this->allocator_, this->get_pointer(), this->size_ + 1);
  }
}

json_key::json_key(std::string_view sv)
    : size_{static_cast<size_type>(sv.size())},
      is_view_{false},
      data_{allocate_string()} {
  copy_string(sv.data(), size_, this->get_pointer());
}

json_key::json_key(self_type const& other)
    : size_{other.size_},
      is_view_{other.is_view_},
      data_{.cptr_ = is_view_ ? other.data_.cptr_ : allocate_string()} {
  if (!is_view_) {
    copy_string(other.get_pointer(), size_, this->get_pointer());
  }
}

json_key& json_key::operator=(self_type const& other) {
  this->size_ = other.size_;
  this->is_view_ = other.is_view_;
  this->data_.cptr_ = this->is_view_ ? other.data_.cptr_ : allocate_string();
  if (!this->is_view_) {
    copy_string(other.get_pointer(), this->size_, this->get_pointer());
  }
  return *this;
}

json_key::json_key(self_type&& other) noexcept
    : size_{other.size_},
      is_view_{other.is_view_},
      data_{.cptr_ = other.on_sso()
                         ? nullptr
                         : std::exchange(other.data_.cptr_, nullptr)} {
  if (this->on_sso()) {
    copy_string(other.sso_, size_, this->sso_);
  }
}

json_key& json_key::operator=(self_type&& other) noexcept {
  this->size_ = other.size_;
  this->is_view_ = other.is_view_;
  if (other.on_sso()) {
    copy_string(other.sso_, size_, this->sso_);
  } else {
    this->data_.cptr_ = std::exchange(other.data_.cptr_, nullptr);
  }
  return *this;
}

json_key::~json_key() noexcept { destroy_string(); }

std::string_view json_key::get() const noexcept {
  return {this->get_pointer(), this->size_};
}

json_key::size_type json_key::size() const noexcept { return this->size_; }
json_key::size_type json_key::max_size() const noexcept {
  return std::numeric_limits<size_type>::max();
}

json_key::operator std::string_view() const noexcept { return this->get(); }

std::ostream& operator<<(std::ostream& os, json_key const& obj) {
  os << '"';
  if (obj.is_view_) {
    os.write(obj.get_pointer(), obj.size_);
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

}  // namespace lazy