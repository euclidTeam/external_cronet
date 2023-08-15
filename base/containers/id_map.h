// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_ID_MAP_H_
#define BASE_CONTAINERS_ID_MAP_H_

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"

namespace base {

// This object maintains a list of IDs that can be quickly converted to
// pointers to objects. It is implemented as a hash table, optimized for
// relatively small data sets (in the common case, there will be exactly one
// item in the list).
//
// Items can be inserted into the container with arbitrary ID, but the caller
// must ensure they are unique. Inserting IDs and relying on automatically
// generated ones is not allowed because they can collide.
//
// The map's value type (the V param) can be any dereferenceable type, such as a
// raw pointer or smart pointer, and must be comparable with nullptr.
template <typename V, typename K = int32_t>
class IDMap final {
 public:
  using KeyType = K;

 private:
  // The value type `V` must be pointer-like and support operator*.
  using T = typename std::remove_reference<decltype(*V())>::type;

  using HashTable = std::unordered_map<KeyType, V>;

 public:
  IDMap() : iteration_depth_(0), next_id_(1), check_on_null_data_(false) {
    // A number of consumers of IDMap create it on one thread but always
    // access it from a different, but consistent, thread (or sequence)
    // post-construction. The first call to CalledOnValidSequence() will re-bind
    // it.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  IDMap(const IDMap&) = delete;
  IDMap& operator=(const IDMap&) = delete;

  ~IDMap() {
    // Many IDMap's are static, and hence will be destroyed on the main
    // thread. However, all the accesses may take place on another thread (or
    // sequence), such as the IO thread. Detaching again to clean this up.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // Sets whether Add and Replace should DCHECK if passed in NULL data.
  // Default is false.
  void set_check_on_null_data(bool value) { check_on_null_data_ = value; }

  // Adds a view with an automatically generated unique ID. See AddWithID.
  //
  // The generated key comes from the template type `K`, with each key being
  // generated by incrementing `K`. The key type should not generate duplicate
  // keys or this function can CHECK-fail.
  KeyType Add(V data) { return AddInternal(std::move(data)); }

  // Adds a new data member with the specified ID. The ID must not be in
  // the list. The caller either must generate all unique IDs itself and use
  // this function, or allow this object to generate IDs and call Add. These
  // two methods may not be mixed, or duplicate IDs may be generated.
  void AddWithID(V data, KeyType id) { AddWithIDInternal(std::move(data), id); }

  // Removes the `id` from the map.
  //
  // Does nothing if the `id` is not in the map.
  void Remove(KeyType id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    typename HashTable::iterator i = data_.find(id);
    if (i == data_.end() || IsRemoved(id)) {
      return;
    }

    if (iteration_depth_ == 0) {
      data_.erase(i);
    } else {
      removed_ids_.insert(id);
    }
  }

  // Replaces the value for `id` with `new_data` and returns the existing value.
  //
  // May only be called with an id that is in the map, and will CHECK()
  // otherwise. It is up to the caller to keep track whether the `id` is in the
  // map, as Lookup() can return null for ids that are in the map but have an
  // empty value.
  V Replace(KeyType id, V new_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!check_on_null_data_ || new_data);
    typename HashTable::iterator i = data_.find(id);
    CHECK(i != data_.end());
    CHECK(!IsRemoved(id));

    using std::swap;
    swap(i->second, new_data);
    return new_data;
  }

  void Clear() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (iteration_depth_ == 0) {
      data_.clear();
    } else {
      removed_ids_.reserve(data_.size());
      removed_ids_.insert(KeyIterator(data_.begin()), KeyIterator(data_.end()));
    }
  }

  bool IsEmpty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return size() == 0u;
  }

  // Returns a pointer to raw value associated with `id` if the `id` is in the
  // map and is not empty.
  //
  // The raw value is obtained by dereferencing the stored value type `V`.
  //
  // If the `id` is not in the map, or the value type compares as equal to
  // nullptr, this function will return null.
  T* Lookup(KeyType id) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    typename HashTable::const_iterator i = data_.find(id);
    if (i == data_.end() || IsRemoved(id)) {
      return nullptr;
    }
    // The IDMap contains a pointer or pointer-like object. We don't want to
    // dereference null, so this acts as an extension point for
    // IDMap, where if the value object compares as equal to nullptr, it its
    // dereference type will not be returned from the map.
    if (i->second == nullptr) {
      return nullptr;
    }
    return std::addressof(*i->second);
  }

  size_t size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_.size() - removed_ids_.size();
  }

#if defined(UNIT_TEST)
  int iteration_depth() const { return iteration_depth_; }
#endif  // defined(UNIT_TEST)

  // It is safe to remove elements from the map during iteration. All iterators
  // will remain valid.
  template <class ReturnType>
  class Iterator {
   public:
    Iterator(IDMap<V, K>* map) : map_(map), iter_(map_->data_.begin()) {
      Init();
    }

    Iterator(const Iterator& iter) : map_(iter.map_), iter_(iter.iter_) {
      Init();
    }

    const Iterator& operator=(const Iterator& iter) {
      map_ = iter.map;
      iter_ = iter.iter;
      Init();
      return *this;
    }

    ~Iterator() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(map_->sequence_checker_);

      if (--map_->iteration_depth_ == 0) {
        map_->Compact();
      } else {
        // The iteration depth should not become negative, it would mean there
        // was an untracked iterator which is now being destroyed, and the
        // Compact() call would have happened while an iterator was live.
        CHECK_GT(map_->iteration_depth_, 0);
      }
    }

    bool IsAtEnd() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(map_->sequence_checker_);
      return iter_ == map_->data_.end();
    }

    KeyType GetCurrentKey() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(map_->sequence_checker_);
      return iter_->first;
    }

    ReturnType* GetCurrentValue() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(map_->sequence_checker_);
      if (!iter_->second || map_->IsRemoved(iter_->first)) {
        return nullptr;
      }
      return &*iter_->second;
    }

    void Advance() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(map_->sequence_checker_);
      ++iter_;
      SkipRemovedEntries();
    }

   private:
    void Init() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(map_->sequence_checker_);
      // Guard signed integer overflow.
      CHECK(map_->iteration_depth_ < std::numeric_limits<int>::max());
      ++map_->iteration_depth_;
      SkipRemovedEntries();
    }

    void SkipRemovedEntries() {
      while (iter_ != map_->data_.end() && map_->IsRemoved(iter_->first)) {
        ++iter_;
      }
    }

    raw_ptr<IDMap<V, K>> map_;
    typename HashTable::const_iterator iter_;
  };

  typedef Iterator<T> iterator;
  typedef Iterator<const T> const_iterator;

 private:
  // Transforms a map iterator to an iterator on the keys of the map.
  // Used by Clear() to populate |removed_ids_| in bulk.
  struct KeyIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = KeyType;
    using difference_type = std::ptrdiff_t;
    using pointer = KeyType*;
    using reference = KeyType&;

    using inner_iterator = typename HashTable::iterator;
    inner_iterator iter_;

    KeyIterator(inner_iterator iter) : iter_(iter) {}
    KeyType operator*() const { return iter_->first; }
    KeyIterator& operator++() {
      ++iter_;
      return *this;
    }
    KeyIterator operator++(int) { return KeyIterator(iter_++); }
    bool operator==(const KeyIterator& other) const {
      return iter_ == other.iter_;
    }
    bool operator!=(const KeyIterator& other) const {
      return iter_ != other.iter_;
    }
  };

  KeyType AddInternal(V data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!check_on_null_data_ || data);
    KeyType this_id = next_id_;

    AddWithIDInternal(std::move(data), this_id);

    if constexpr (std::is_integral_v<K>) {
      // Guard signed integer overflow, and duplicate unsigned keys.
      CHECK(next_id_ < std::numeric_limits<K>::max());
    }
    next_id_++;

    return this_id;
  }

  void AddWithIDInternal(V data, KeyType id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!check_on_null_data_ || data);
    if (IsRemoved(id)) {
      removed_ids_.erase(id);
      data_[id] = std::move(data);
    } else {
      auto [_, inserted] = data_.emplace(id, std::move(data));
      CHECK(inserted) << "Inserting duplicate item";
    }
  }

  bool IsRemoved(KeyType key) const {
    return removed_ids_.find(key) != removed_ids_.end();
  }

  void Compact() {
    DCHECK_EQ(0, iteration_depth_);
    for (const auto& i : removed_ids_) {
      data_.erase(i);
    }
    removed_ids_.clear();
  }

  // Keep track of how many iterators are currently iterating on us to safely
  // handle removing items during iteration.
  int iteration_depth_;

  // Keep set of IDs that should be removed after the outermost iteration has
  // finished. This way we manage to not invalidate the iterator when an element
  // is removed.
  base::flat_set<KeyType> removed_ids_;

  // The next ID that we will return from Add()
  KeyType next_id_;

  HashTable data_;

  // See description above setter.
  bool check_on_null_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace base

#endif  // BASE_CONTAINERS_ID_MAP_H_
