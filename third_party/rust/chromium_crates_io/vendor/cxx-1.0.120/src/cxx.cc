#include "third_party/rust/cxx/v1/cxx.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>

#ifndef CXX_RS_EXPORT
#define CXX_RS_EXPORT
#endif
#ifndef CXX_CPP_EXPORT
#define CXX_CPP_EXPORT
#endif

extern "C" {
CXX_RS_EXPORT void cxxbridge1$cxx_string$init(std::string *s,
                                              const std::uint8_t *ptr,
                                              std::size_t len) noexcept {
  new (s) std::string(reinterpret_cast<const char *>(ptr), len);
}

CXX_RS_EXPORT void cxxbridge1$cxx_string$destroy(std::string *s) noexcept {
  using std::string;
  s->~string();
}

CXX_RS_EXPORT const char *
cxxbridge1$cxx_string$data(const std::string &s) noexcept {
  return s.data();
}

CXX_RS_EXPORT std::size_t
cxxbridge1$cxx_string$length(const std::string &s) noexcept {
  return s.length();
}

CXX_RS_EXPORT void cxxbridge1$cxx_string$clear(std::string &s) noexcept {
  s.clear();
}

CXX_RS_EXPORT void
cxxbridge1$cxx_string$reserve_total(std::string &s, size_t new_cap) noexcept {
  s.reserve(new_cap);
}

CXX_RS_EXPORT void cxxbridge1$cxx_string$push(std::string &s,
                                              const std::uint8_t *ptr,
                                              std::size_t len) noexcept {
  s.append(reinterpret_cast<const char *>(ptr), len);
}

// rust::String
CXX_RS_EXPORT void cxxbridge1$string$new(rust::String *self) noexcept;
CXX_RS_EXPORT void cxxbridge1$string$clone(rust::String *self,
                                           const rust::String &other) noexcept;
CXX_RS_EXPORT bool cxxbridge1$string$from_utf8(rust::String *self,
                                               const char *ptr,
                                               std::size_t len) noexcept;
CXX_RS_EXPORT void cxxbridge1$string$from_utf8_lossy(rust::String *self,
                                                     const char *ptr,
                                                     std::size_t len) noexcept;
CXX_RS_EXPORT bool cxxbridge1$string$from_utf16(rust::String *self,
                                                const char16_t *ptr,
                                                std::size_t len) noexcept;
CXX_RS_EXPORT void cxxbridge1$string$from_utf16_lossy(rust::String *self,
                                                      const char16_t *ptr,
                                                      std::size_t len) noexcept;
CXX_RS_EXPORT void cxxbridge1$string$drop(rust::String *self) noexcept;
CXX_RS_EXPORT const char *
cxxbridge1$string$ptr(const rust::String *self) noexcept;
CXX_RS_EXPORT std::size_t
cxxbridge1$string$len(const rust::String *self) noexcept;
CXX_RS_EXPORT std::size_t
cxxbridge1$string$capacity(const rust::String *self) noexcept;
CXX_RS_EXPORT void
cxxbridge1$string$reserve_additional(rust::String *self,
                                     size_t additional) noexcept;
CXX_RS_EXPORT void cxxbridge1$string$reserve_total(rust::String *self,
                                                   size_t new_cap) noexcept;

// rust::Str
CXX_RS_EXPORT void cxxbridge1$str$new(rust::Str *self) noexcept;
CXX_RS_EXPORT void cxxbridge1$str$ref(rust::Str *self,
                                      const rust::String *string) noexcept;
CXX_RS_EXPORT bool cxxbridge1$str$from(rust::Str *self, const char *ptr,
                                       std::size_t len) noexcept;
CXX_RS_EXPORT const char *cxxbridge1$str$ptr(const rust::Str *self) noexcept;
CXX_RS_EXPORT std::size_t cxxbridge1$str$len(const rust::Str *self) noexcept;

// rust::Slice
CXX_RS_EXPORT void cxxbridge1$slice$new(void *self, const void *ptr,
                                        std::size_t len) noexcept;
CXX_RS_EXPORT void *cxxbridge1$slice$ptr(const void *self) noexcept;
CXX_RS_EXPORT std::size_t cxxbridge1$slice$len(const void *self) noexcept;
} // extern "C"

namespace rust {
inline namespace cxxbridge1 {

template <typename Exception>
void panic [[noreturn]] (const char *msg) {
#if defined(RUST_CXX_NO_EXCEPTIONS)
  std::fprintf(stderr, "Error: %s. Aborting.\n", msg);
  std::abort();
#else
  throw Exception(msg);
#endif
}

template void panic<std::out_of_range> [[noreturn]] (const char *msg);


template <typename T>
static bool is_aligned(const void *ptr) noexcept {
  auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
  return !(iptr % alignof(T));
}

CXX_CPP_EXPORT String::String() noexcept { cxxbridge1$string$new(this); }

CXX_CPP_EXPORT String::String(const String &other) noexcept {
  cxxbridge1$string$clone(this, other);
}

CXX_CPP_EXPORT String::String(String &&other) noexcept : repr(other.repr) {
  cxxbridge1$string$new(&other);
}

CXX_CPP_EXPORT String::~String() noexcept { cxxbridge1$string$drop(this); }

static void initString(String *self, const char *s, std::size_t len) {
  if (!cxxbridge1$string$from_utf8(self, s, len)) {
    panic<std::invalid_argument>("data for rust::String is not utf-8");
  }
}

static void initString(String *self, const char16_t *s, std::size_t len) {
  if (!cxxbridge1$string$from_utf16(self, s, len)) {
    panic<std::invalid_argument>("data for rust::String is not utf-16");
  }
}

CXX_CPP_EXPORT String::String(const std::string &s) {
  initString(this, s.data(), s.length());
}

CXX_CPP_EXPORT String::String(const char *s) {
  assert(s != nullptr);
  initString(this, s, std::strlen(s));
}

CXX_CPP_EXPORT String::String(const char *s, std::size_t len) {
  assert(s != nullptr || len == 0);
  initString(this,
             s == nullptr && len == 0 ? reinterpret_cast<const char *>(1) : s,
             len);
}

CXX_CPP_EXPORT String::String(const char16_t *s) {
  assert(s != nullptr);
  assert(is_aligned<char16_t>(s));
  initString(this, s, std::char_traits<char16_t>::length(s));
}

CXX_CPP_EXPORT String::String(const char16_t *s, std::size_t len) {
  assert(s != nullptr || len == 0);
  assert(is_aligned<char16_t>(s));
  initString(this,
             s == nullptr && len == 0 ? reinterpret_cast<const char16_t *>(2)
                                      : s,
             len);
}

struct String::lossy_t {};

CXX_CPP_EXPORT String::String(lossy_t, const char *s,
                              std::size_t len) noexcept {
  cxxbridge1$string$from_utf8_lossy(
      this, s == nullptr && len == 0 ? reinterpret_cast<const char *>(1) : s,
      len);
}

CXX_CPP_EXPORT String::String(lossy_t, const char16_t *s,
                              std::size_t len) noexcept {
  cxxbridge1$string$from_utf16_lossy(
      this,
      s == nullptr && len == 0 ? reinterpret_cast<const char16_t *>(2) : s,
      len);
}

CXX_CPP_EXPORT String String::lossy(const std::string &s) noexcept {
  return String::lossy(s.data(), s.length());
}

CXX_CPP_EXPORT String String::lossy(const char *s) noexcept {
  assert(s != nullptr);
  return String::lossy(s, std::strlen(s));
}

CXX_CPP_EXPORT String String::lossy(const char *s, std::size_t len) noexcept {
  assert(s != nullptr || len == 0);
  return String(lossy_t{}, s, len);
}

CXX_CPP_EXPORT String String::lossy(const char16_t *s) noexcept {
  assert(s != nullptr);
  assert(is_aligned<char16_t>(s));
  return String(lossy_t{}, s, std::char_traits<char16_t>::length(s));
}

CXX_CPP_EXPORT String String::lossy(const char16_t *s,
                                    std::size_t len) noexcept {
  assert(s != nullptr || len == 0);
  assert(is_aligned<char16_t>(s));
  return String(lossy_t{}, s, len);
}

CXX_CPP_EXPORT String &String::operator=(const String &other) &noexcept {
  if (this != &other) {
    cxxbridge1$string$drop(this);
    cxxbridge1$string$clone(this, other);
  }
  return *this;
}

CXX_CPP_EXPORT String &String::operator=(String &&other) &noexcept {
  cxxbridge1$string$drop(this);
  this->repr = other.repr;
  cxxbridge1$string$new(&other);
  return *this;
}

CXX_CPP_EXPORT String::operator std::string() const {
  return std::string(this->data(), this->size());
}

CXX_CPP_EXPORT const char *String::data() const noexcept {
  return cxxbridge1$string$ptr(this);
}

CXX_CPP_EXPORT std::size_t String::size() const noexcept {
  return cxxbridge1$string$len(this);
}

CXX_CPP_EXPORT std::size_t String::length() const noexcept {
  return cxxbridge1$string$len(this);
}

CXX_CPP_EXPORT bool String::empty() const noexcept { return this->size() == 0; }

CXX_CPP_EXPORT const char *String::c_str() noexcept {
  auto len = this->length();
  cxxbridge1$string$reserve_additional(this, 1);
  auto ptr = this->data();
  const_cast<char *>(ptr)[len] = '\0';
  return ptr;
}

CXX_CPP_EXPORT std::size_t String::capacity() const noexcept {
  return cxxbridge1$string$capacity(this);
}

CXX_CPP_EXPORT void String::reserve(std::size_t new_cap) noexcept {
  cxxbridge1$string$reserve_total(this, new_cap);
}

CXX_CPP_EXPORT String::iterator String::begin() noexcept {
  return const_cast<char *>(this->data());
}

CXX_CPP_EXPORT String::iterator String::end() noexcept {
  return const_cast<char *>(this->data()) + this->size();
}

CXX_CPP_EXPORT String::const_iterator String::begin() const noexcept { return this->cbegin(); }

CXX_CPP_EXPORT String::const_iterator String::end() const noexcept { return this->cend(); }

CXX_CPP_EXPORT String::const_iterator String::cbegin() const noexcept { return this->data(); }

CXX_CPP_EXPORT String::const_iterator String::cend() const noexcept {
  return this->data() + this->size();
}

CXX_CPP_EXPORT bool String::operator==(const String &rhs) const noexcept {
  return rust::Str(*this) == rust::Str(rhs);
}

CXX_CPP_EXPORT bool String::operator!=(const String &rhs) const noexcept {
  return rust::Str(*this) != rust::Str(rhs);
}

CXX_CPP_EXPORT bool String::operator<(const String &rhs) const noexcept {
  return rust::Str(*this) < rust::Str(rhs);
}

CXX_CPP_EXPORT bool String::operator<=(const String &rhs) const noexcept {
  return rust::Str(*this) <= rust::Str(rhs);
}

CXX_CPP_EXPORT bool String::operator>(const String &rhs) const noexcept {
  return rust::Str(*this) > rust::Str(rhs);
}

CXX_CPP_EXPORT bool String::operator>=(const String &rhs) const noexcept {
  return rust::Str(*this) >= rust::Str(rhs);
}

CXX_CPP_EXPORT void String::swap(String &rhs) noexcept {
  using std::swap;
  swap(this->repr, rhs.repr);
}

CXX_CPP_EXPORT String::String(unsafe_bitcopy_t, const String &bits) noexcept
    : repr(bits.repr) {}

CXX_CPP_EXPORT std::ostream &operator<<(std::ostream &os, const String &s) {
  os.write(s.data(), static_cast<std::streamsize>(s.size()));
  return os;
}

CXX_CPP_EXPORT Str::Str() noexcept { cxxbridge1$str$new(this); }

CXX_CPP_EXPORT Str::Str(const String &s) noexcept { cxxbridge1$str$ref(this, &s); }

static void initStr(Str *self, const char *ptr, std::size_t len) {
  if (!cxxbridge1$str$from(self, ptr, len)) {
    panic<std::invalid_argument>("data for rust::Str is not utf-8");
  }
}

CXX_CPP_EXPORT Str::Str(const std::string &s) { initStr(this, s.data(), s.length()); }

CXX_CPP_EXPORT Str::Str(const char *s) {
  assert(s != nullptr);
  initStr(this, s, std::strlen(s));
}

CXX_CPP_EXPORT Str::Str(const char *s, std::size_t len) {
  assert(s != nullptr || len == 0);
  initStr(this,
          s == nullptr && len == 0 ? reinterpret_cast<const char *>(1) : s,
          len);
}

CXX_CPP_EXPORT Str::operator std::string() const {
  return std::string(this->data(), this->size());
}

CXX_CPP_EXPORT const char *Str::data() const noexcept { return cxxbridge1$str$ptr(this); }

CXX_CPP_EXPORT std::size_t Str::size() const noexcept { return cxxbridge1$str$len(this); }

CXX_CPP_EXPORT std::size_t Str::length() const noexcept { return this->size(); }

CXX_CPP_EXPORT bool Str::empty() const noexcept { return this->size() == 0; }

CXX_CPP_EXPORT Str::const_iterator Str::begin() const noexcept { return this->cbegin(); }

CXX_CPP_EXPORT Str::const_iterator Str::end() const noexcept { return this->cend(); }

CXX_CPP_EXPORT Str::const_iterator Str::cbegin() const noexcept { return this->data(); }

CXX_CPP_EXPORT Str::const_iterator Str::cend() const noexcept {
  return this->data() + this->size();
}

CXX_CPP_EXPORT bool Str::operator==(const Str &rhs) const noexcept {
  return this->size() == rhs.size() &&
         std::equal(this->begin(), this->end(), rhs.begin());
}

CXX_CPP_EXPORT bool Str::operator!=(const Str &rhs) const noexcept { return !(*this == rhs); }

CXX_CPP_EXPORT bool Str::operator<(const Str &rhs) const noexcept {
  return std::lexicographical_compare(this->begin(), this->end(), rhs.begin(),
                                      rhs.end());
}

CXX_CPP_EXPORT bool Str::operator<=(const Str &rhs) const noexcept {
  // std::mismatch(this->begin(), this->end(), rhs.begin(), rhs.end()), except
  // without Undefined Behavior on C++11 if rhs is shorter than *this.
  const_iterator liter = this->begin(), lend = this->end(), riter = rhs.begin(),
                 rend = rhs.end();
  while (liter != lend && riter != rend && *liter == *riter) {
    ++liter, ++riter;
  }
  if (liter == lend) {
    return true; // equal or *this is a prefix of rhs
  } else if (riter == rend) {
    return false; // rhs is a prefix of *this
  } else {
    return *liter <= *riter;
  }
}

CXX_CPP_EXPORT bool Str::operator>(const Str &rhs) const noexcept { return rhs < *this; }

CXX_CPP_EXPORT bool Str::operator>=(const Str &rhs) const noexcept { return rhs <= *this; }

CXX_CPP_EXPORT void Str::swap(Str &rhs) noexcept {
  using std::swap;
  swap(this->repr, rhs.repr);
}

CXX_CPP_EXPORT std::ostream &operator<<(std::ostream &os, const Str &s) {
  os.write(s.data(), static_cast<std::streamsize>(s.size()));
  return os;
}

CXX_CPP_EXPORT void sliceInit(void *self, const void *ptr, std::size_t len) noexcept {
  cxxbridge1$slice$new(self, ptr, len);
}

CXX_CPP_EXPORT void *slicePtr(const void *self) noexcept { return cxxbridge1$slice$ptr(self); }

CXX_CPP_EXPORT std::size_t sliceLen(const void *self) noexcept {
  return cxxbridge1$slice$len(self);
}

// Rust specifies that usize is ABI compatible with C's uintptr_t.
// https://rust-lang.github.io/unsafe-code-guidelines/layout/scalars.html#isize-and-usize
// However there is no direct Rust equivalent for size_t. C does not guarantee
// that size_t and uintptr_t are compatible. In practice though, on all
// platforms supported by Rust, they are identical for ABI purposes. See the
// libc crate which unconditionally defines libc::size_t = usize. We expect the
// same here and these assertions are just here to explicitly document that.
// *Note that no assumption is made about C++ name mangling of signatures
// containing these types, not here nor anywhere in CXX.*
static_assert(sizeof(std::size_t) == sizeof(std::uintptr_t),
              "unsupported size_t size");
static_assert(alignof(std::size_t) == alignof(std::uintptr_t),
              "unsupported size_t alignment");
static_assert(sizeof(rust::isize) == sizeof(std::intptr_t),
              "unsupported ssize_t size");
static_assert(alignof(rust::isize) == alignof(std::intptr_t),
              "unsupported ssize_t alignment");

static_assert(std::is_trivially_copy_constructible<Str>::value,
              "trivial Str(const Str &)");
static_assert(std::is_trivially_copy_assignable<Str>::value,
              "trivial operator=(const Str &)");
static_assert(std::is_trivially_destructible<Str>::value, "trivial ~Str()");

static_assert(
    std::is_trivially_copy_constructible<Slice<const std::uint8_t>>::value,
    "trivial Slice(const Slice &)");
static_assert(
    std::is_trivially_move_constructible<Slice<const std::uint8_t>>::value,
    "trivial Slice(Slice &&)");
static_assert(
    std::is_trivially_copy_assignable<Slice<const std::uint8_t>>::value,
    "trivial Slice::operator=(const Slice &) for const slices");
static_assert(
    std::is_trivially_move_assignable<Slice<const std::uint8_t>>::value,
    "trivial Slice::operator=(Slice &&)");
static_assert(std::is_trivially_destructible<Slice<const std::uint8_t>>::value,
              "trivial ~Slice()");

static_assert(std::is_trivially_copy_constructible<Slice<std::uint8_t>>::value,
              "trivial Slice(const Slice &)");
static_assert(std::is_trivially_move_constructible<Slice<std::uint8_t>>::value,
              "trivial Slice(Slice &&)");
static_assert(!std::is_copy_assignable<Slice<std::uint8_t>>::value,
              "delete Slice::operator=(const Slice &) for mut slices");
static_assert(std::is_trivially_move_assignable<Slice<std::uint8_t>>::value,
              "trivial Slice::operator=(Slice &&)");
static_assert(std::is_trivially_destructible<Slice<std::uint8_t>>::value,
              "trivial ~Slice()");

static_assert(std::is_same<Vec<std::uint8_t>::const_iterator,
                           Vec<const std::uint8_t>::iterator>::value,
              "Vec<T>::const_iterator == Vec<const T>::iterator");
static_assert(std::is_same<Vec<const std::uint8_t>::const_iterator,
                           Vec<const std::uint8_t>::iterator>::value,
              "Vec<const T>::const_iterator == Vec<const T>::iterator");
static_assert(!std::is_same<Vec<std::uint8_t>::const_iterator,
                            Vec<std::uint8_t>::iterator>::value,
              "Vec<T>::const_iterator != Vec<T>::iterator");

static const char *errorCopy(const char *ptr, std::size_t len) {
  char *copy = new char[len];
  std::memcpy(copy, ptr, len);
  return copy;
}

extern "C" {
CXX_RS_EXPORT const char *cxxbridge1$error(const char *ptr,
                                           std::size_t len) noexcept {
  return errorCopy(ptr, len);
}
} // extern "C"

CXX_CPP_EXPORT Error::Error(const Error &other)
    : std::exception(other),
      msg(other.msg ? errorCopy(other.msg, other.len) : nullptr),
      len(other.len) {}

CXX_CPP_EXPORT Error::Error(Error &&other) noexcept
    : std::exception(std::move(other)), msg(other.msg), len(other.len) {
  other.msg = nullptr;
  other.len = 0;
}

CXX_CPP_EXPORT Error::~Error() noexcept { delete[] this->msg; }

CXX_CPP_EXPORT Error &Error::operator=(const Error &other) & {
  if (this != &other) {
    std::exception::operator=(other);
    delete[] this->msg;
    this->msg = nullptr;
    if (other.msg) {
      this->msg = errorCopy(other.msg, other.len);
      this->len = other.len;
    }
  }
  return *this;
}

CXX_CPP_EXPORT Error &Error::operator=(Error &&other) &noexcept {
  std::exception::operator=(std::move(other));
  delete[] this->msg;
  this->msg = other.msg;
  this->len = other.len;
  other.msg = nullptr;
  other.len = 0;
  return *this;
}

CXX_CPP_EXPORT const char *Error::what() const noexcept { return this->msg; }

namespace {
template <typename T>
union MaybeUninit {
  T value;
  MaybeUninit() {}
  ~MaybeUninit() {}
};
} // namespace

namespace repr {
struct PtrLen final {
  void *ptr;
  std::size_t len;
};
} // namespace repr

extern "C" {
CXX_RS_EXPORT repr::PtrLen cxxbridge1$exception(const char *, std::size_t len) noexcept;
}

namespace detail {
// On some platforms size_t is the same C++ type as one of the sized integer
// types; on others it is a distinct type. Only in the latter case do we need to
// define a specialized impl of rust::Vec<size_t>, because in the former case it
// would collide with one of the other specializations.
using usize_if_unique =
    typename std::conditional<std::is_same<size_t, uint64_t>::value ||
                                  std::is_same<size_t, uint32_t>::value,
                              struct usize_ignore, size_t>::type;
using isize_if_unique =
    typename std::conditional<std::is_same<rust::isize, int64_t>::value ||
                                  std::is_same<rust::isize, int32_t>::value,
                              struct isize_ignore, rust::isize>::type;

class Fail final {
  repr::PtrLen &throw$;

public:
  Fail(repr::PtrLen &throw$) noexcept : throw$(throw$) {}
  void operator()(const char *) noexcept;
  void operator()(const std::string &) noexcept;
};

CXX_CPP_EXPORT void Fail::operator()(const char *catch$) noexcept {
  throw$ = cxxbridge1$exception(catch$, std::strlen(catch$));
}

CXX_CPP_EXPORT void Fail::operator()(const std::string &catch$) noexcept {
  throw$ = cxxbridge1$exception(catch$.data(), catch$.length());
}
} // namespace detail

} // namespace cxxbridge1
} // namespace rust

namespace {
template <typename T>
void destroy(T *ptr) {
  ptr->~T();
}
} // namespace

extern "C" {
CXX_RS_EXPORT void cxxbridge1$unique_ptr$std$string$null(
    std::unique_ptr<std::string> *ptr) noexcept {
  new (ptr) std::unique_ptr<std::string>();
}
CXX_RS_EXPORT void
cxxbridge1$unique_ptr$std$string$raw(std::unique_ptr<std::string> *ptr,
                                     std::string *raw) noexcept {
  new (ptr) std::unique_ptr<std::string>(raw);
}
CXX_RS_EXPORT const std::string *cxxbridge1$unique_ptr$std$string$get(
    const std::unique_ptr<std::string> &ptr) noexcept {
  return ptr.get();
}
CXX_RS_EXPORT std::string *cxxbridge1$unique_ptr$std$string$release(
    std::unique_ptr<std::string> &ptr) noexcept {
  return ptr.release();
}
CXX_RS_EXPORT void cxxbridge1$unique_ptr$std$string$drop(
    std::unique_ptr<std::string> *ptr) noexcept {
  ptr->~unique_ptr();
}
} // extern "C"

namespace {
const std::size_t kMaxExpectedWordsInString = 8;
static_assert(alignof(std::string) <= alignof(void *),
              "unexpectedly large std::string alignment");
static_assert(sizeof(std::string) <= kMaxExpectedWordsInString * sizeof(void *),
              "unexpectedly large std::string size");
} // namespace

#define STD_VECTOR_OPS(RUST_TYPE, CXX_TYPE)                                    \
  CXX_RS_EXPORT std::vector<CXX_TYPE> *cxxbridge1$std$vector$##RUST_TYPE##$new() noexcept {  \
    return new std::vector<CXX_TYPE>();                                        \
  }                                                                            \
  CXX_RS_EXPORT std::size_t cxxbridge1$std$vector$##RUST_TYPE##$size(                        \
      const std::vector<CXX_TYPE> &s) noexcept {                               \
    return s.size();                                                           \
  }                                                                            \
  CXX_RS_EXPORT CXX_TYPE *cxxbridge1$std$vector$##RUST_TYPE##$get_unchecked(                 \
      std::vector<CXX_TYPE> *s, std::size_t pos) noexcept {                    \
    return &(*s)[pos];                                                         \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$unique_ptr$std$vector$##RUST_TYPE##$null(                    \
      std::unique_ptr<std::vector<CXX_TYPE>> *ptr) noexcept {                  \
    new (ptr) std::unique_ptr<std::vector<CXX_TYPE>>();                        \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$unique_ptr$std$vector$##RUST_TYPE##$raw(                     \
      std::unique_ptr<std::vector<CXX_TYPE>> *ptr,                             \
      std::vector<CXX_TYPE> *raw) noexcept {                                   \
    new (ptr) std::unique_ptr<std::vector<CXX_TYPE>>(raw);                     \
  }                                                                            \
  CXX_RS_EXPORT const std::vector<CXX_TYPE>                                                  \
      *cxxbridge1$unique_ptr$std$vector$##RUST_TYPE##$get(                     \
          const std::unique_ptr<std::vector<CXX_TYPE>> &ptr) noexcept {        \
    return ptr.get();                                                          \
  }                                                                            \
  CXX_RS_EXPORT std::vector<CXX_TYPE>                                                        \
      *cxxbridge1$unique_ptr$std$vector$##RUST_TYPE##$release(                 \
          std::unique_ptr<std::vector<CXX_TYPE>> &ptr) noexcept {              \
    return ptr.release();                                                      \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$unique_ptr$std$vector$##RUST_TYPE##$drop(                    \
      std::unique_ptr<std::vector<CXX_TYPE>> *ptr) noexcept {                  \
    ptr->~unique_ptr();                                                        \
  }

#define STD_VECTOR_TRIVIAL_OPS(RUST_TYPE, CXX_TYPE)                            \
  CXX_RS_EXPORT void cxxbridge1$std$vector$##RUST_TYPE##$push_back(                          \
      std::vector<CXX_TYPE> *v, CXX_TYPE *value) noexcept {                    \
    v->push_back(std::move(*value));                                           \
    destroy(value);                                                            \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$std$vector$##RUST_TYPE##$pop_back(std::vector<CXX_TYPE> *v,  \
                                                    CXX_TYPE *out) noexcept {  \
    new (out) CXX_TYPE(std::move(v->back()));                                  \
    v->pop_back();                                                             \
  }

#define RUST_VEC_EXTERNS(RUST_TYPE, CXX_TYPE)                                  \
  CXX_RS_EXPORT void cxxbridge1$rust_vec$##RUST_TYPE##$new(                                  \
      rust::Vec<CXX_TYPE> *ptr) noexcept;                                      \
  CXX_RS_EXPORT void cxxbridge1$rust_vec$##RUST_TYPE##$drop(                                 \
      rust::Vec<CXX_TYPE> *ptr) noexcept;                                      \
  CXX_RS_EXPORT std::size_t cxxbridge1$rust_vec$##RUST_TYPE##$len(                           \
      const rust::Vec<CXX_TYPE> *ptr) noexcept;                                \
  CXX_RS_EXPORT std::size_t cxxbridge1$rust_vec$##RUST_TYPE##$capacity(                      \
      const rust::Vec<CXX_TYPE> *ptr) noexcept;                                \
  CXX_RS_EXPORT const CXX_TYPE *cxxbridge1$rust_vec$##RUST_TYPE##$data(                      \
      const rust::Vec<CXX_TYPE> *ptr) noexcept;                                \
  CXX_RS_EXPORT void cxxbridge1$rust_vec$##RUST_TYPE##$reserve_total(                        \
      rust::Vec<CXX_TYPE> *ptr, std::size_t new_cap) noexcept;                 \
  CXX_RS_EXPORT void cxxbridge1$rust_vec$##RUST_TYPE##$set_len(rust::Vec<CXX_TYPE> *ptr,     \
                                                 std::size_t len) noexcept;    \
  CXX_RS_EXPORT void cxxbridge1$rust_vec$##RUST_TYPE##$truncate(rust::Vec<CXX_TYPE> *ptr,    \
                                                  std::size_t len) noexcept;

#define RUST_VEC_OPS(RUST_TYPE, CXX_TYPE)                                      \
  template <>                                                                  \
  Vec<CXX_TYPE>::Vec() noexcept {                                              \
    cxxbridge1$rust_vec$##RUST_TYPE##$new(this);                               \
  }                                                                            \
  template <>                                                                  \
  void Vec<CXX_TYPE>::drop() noexcept {                                        \
    return cxxbridge1$rust_vec$##RUST_TYPE##$drop(this);                       \
  }                                                                            \
  template <>                                                                  \
  std::size_t Vec<CXX_TYPE>::size() const noexcept {                           \
    return cxxbridge1$rust_vec$##RUST_TYPE##$len(this);                        \
  }                                                                            \
  template <>                                                                  \
  std::size_t Vec<CXX_TYPE>::capacity() const noexcept {                       \
    return cxxbridge1$rust_vec$##RUST_TYPE##$capacity(this);                   \
  }                                                                            \
  template <>                                                                  \
  const CXX_TYPE *Vec<CXX_TYPE>::data() const noexcept {                       \
    return cxxbridge1$rust_vec$##RUST_TYPE##$data(this);                       \
  }                                                                            \
  template <>                                                                  \
  void Vec<CXX_TYPE>::reserve_total(std::size_t new_cap) noexcept {            \
    cxxbridge1$rust_vec$##RUST_TYPE##$reserve_total(this, new_cap);            \
  }                                                                            \
  template <>                                                                  \
  void Vec<CXX_TYPE>::set_len(std::size_t len) noexcept {                      \
    cxxbridge1$rust_vec$##RUST_TYPE##$set_len(this, len);                      \
  }                                                                            \
  template <>                                                                  \
  void Vec<CXX_TYPE>::truncate(std::size_t len) {                              \
    cxxbridge1$rust_vec$##RUST_TYPE##$truncate(this, len);                     \
  }

#define SHARED_PTR_OPS(RUST_TYPE, CXX_TYPE)                                    \
  static_assert(sizeof(std::shared_ptr<CXX_TYPE>) == 2 * sizeof(void *), "");  \
  static_assert(alignof(std::shared_ptr<CXX_TYPE>) == alignof(void *), "");    \
  CXX_RS_EXPORT void cxxbridge1$std$shared_ptr$##RUST_TYPE##$null(             \
      std::shared_ptr<CXX_TYPE> *ptr) noexcept {                               \
    new (ptr) std::shared_ptr<CXX_TYPE>();                                     \
  }                                                                            \
  CXX_RS_EXPORT CXX_TYPE *cxxbridge1$std$shared_ptr$##RUST_TYPE##$uninit(      \
      std::shared_ptr<CXX_TYPE> *ptr) noexcept {                               \
    CXX_TYPE *uninit =                                                         \
        reinterpret_cast<CXX_TYPE *>(new rust::MaybeUninit<CXX_TYPE>);         \
    new (ptr) std::shared_ptr<CXX_TYPE>(uninit);                               \
    return uninit;                                                             \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$std$shared_ptr$##RUST_TYPE##$clone(            \
      const std::shared_ptr<CXX_TYPE> &self,                                   \
      std::shared_ptr<CXX_TYPE> *ptr) noexcept {                               \
    new (ptr) std::shared_ptr<CXX_TYPE>(self);                                 \
  }                                                                            \
  CXX_RS_EXPORT const CXX_TYPE *cxxbridge1$std$shared_ptr$##RUST_TYPE##$get(   \
      const std::shared_ptr<CXX_TYPE> &self) noexcept {                        \
    return self.get();                                                         \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$std$shared_ptr$##RUST_TYPE##$drop(             \
      const std::shared_ptr<CXX_TYPE> *self) noexcept {                        \
    self->~shared_ptr();                                                       \
  }                                                                            \
  static_assert(sizeof(std::weak_ptr<CXX_TYPE>) == 2 * sizeof(void *), "");    \
  static_assert(alignof(std::weak_ptr<CXX_TYPE>) == alignof(void *), "");      \
  CXX_RS_EXPORT void cxxbridge1$std$weak_ptr$##RUST_TYPE##$null(               \
      std::weak_ptr<CXX_TYPE> *ptr) noexcept {                                 \
    new (ptr) std::weak_ptr<CXX_TYPE>();                                       \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$std$weak_ptr$##RUST_TYPE##$clone(              \
      const std::weak_ptr<CXX_TYPE> &self,                                     \
      std::weak_ptr<CXX_TYPE> *ptr) noexcept {                                 \
    new (ptr) std::weak_ptr<CXX_TYPE>(self);                                   \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$std$weak_ptr$##RUST_TYPE##$downgrade(          \
      const std::shared_ptr<CXX_TYPE> &shared,                                 \
      std::weak_ptr<CXX_TYPE> *weak) noexcept {                                \
    new (weak) std::weak_ptr<CXX_TYPE>(shared);                                \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$std$weak_ptr$##RUST_TYPE##$upgrade(            \
      const std::weak_ptr<CXX_TYPE> &weak,                                     \
      std::shared_ptr<CXX_TYPE> *shared) noexcept {                            \
    new (shared) std::shared_ptr<CXX_TYPE>(weak.lock());                       \
  }                                                                            \
  CXX_RS_EXPORT void cxxbridge1$std$weak_ptr$##RUST_TYPE##$drop(               \
      const std::weak_ptr<CXX_TYPE> *self) noexcept {                          \
    self->~weak_ptr();                                                         \
  }

// Usize and isize are the same type as one of the below.
#define FOR_EACH_NUMERIC(MACRO)                                                \
  MACRO(u8, std::uint8_t)                                                      \
  MACRO(u16, std::uint16_t)                                                    \
  MACRO(u32, std::uint32_t)                                                    \
  MACRO(u64, std::uint64_t)                                                    \
  MACRO(i8, std::int8_t)                                                       \
  MACRO(i16, std::int16_t)                                                     \
  MACRO(i32, std::int32_t)                                                     \
  MACRO(i64, std::int64_t)                                                     \
  MACRO(f32, float)                                                            \
  MACRO(f64, double)

#define FOR_EACH_TRIVIAL_STD_VECTOR(MACRO)                                     \
  FOR_EACH_NUMERIC(MACRO)                                                      \
  MACRO(usize, std::size_t)                                                    \
  MACRO(isize, rust::isize)

#define FOR_EACH_STD_VECTOR(MACRO)                                             \
  FOR_EACH_TRIVIAL_STD_VECTOR(MACRO)                                           \
  MACRO(string, std::string)

#define FOR_EACH_RUST_VEC(MACRO)                                               \
  FOR_EACH_NUMERIC(MACRO)                                                      \
  MACRO(bool, bool)                                                            \
  MACRO(char, char)                                                            \
  MACRO(usize, rust::detail::usize_if_unique)                                  \
  MACRO(isize, rust::detail::isize_if_unique)                                  \
  MACRO(string, rust::String)                                                  \
  MACRO(str, rust::Str)

#define FOR_EACH_SHARED_PTR(MACRO)                                             \
  FOR_EACH_NUMERIC(MACRO)                                                      \
  MACRO(bool, bool)                                                            \
  MACRO(usize, std::size_t)                                                    \
  MACRO(isize, rust::isize)                                                    \
  MACRO(string, std::string)

extern "C" {
FOR_EACH_STD_VECTOR(STD_VECTOR_OPS)
FOR_EACH_TRIVIAL_STD_VECTOR(STD_VECTOR_TRIVIAL_OPS)
FOR_EACH_RUST_VEC(RUST_VEC_EXTERNS)
FOR_EACH_SHARED_PTR(SHARED_PTR_OPS)
} // extern "C"

namespace rust {
inline namespace cxxbridge1 {
FOR_EACH_RUST_VEC(RUST_VEC_OPS)
} // namespace cxxbridge1
} // namespace rust
