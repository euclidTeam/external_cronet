// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/contains.h"

#include <set>

#include "base/strings/string_piece.h"

namespace base {

// The following code would perform a linear search through the set which is
// likely unexpected and not intended. This is because the expression
// `set.find(kFoo)` is ill-formed, since there is no implimit conversion from
// StringPiece to `std::string`. This means Contains would fall back to the
// general purpose `base::ranges::find(set, kFoo)` linear search.
// To fix this clients can either use a more generic comparator like std::less<>
// (in this case `set.find()` accepts any type that is comparable to a
// std::string), or pass an explicit projection parameter to Contains, at which
// point it will always perform a linear search.
#if defined(NCTEST_CONTAINS_UNEXPECTED_LINEAR_SEARCH)  // [r"Error: About to perform linear search on an associative container."]
void WontCompile() {
  constexpr StringPiece kFoo = "foo";
  std::set<std::string> set = {"foo", "bar", "baz"};
  Contains(set, kFoo);
}
#endif

}  // namespace base
