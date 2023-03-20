//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <algorithm>

#include "common.h"

namespace {
template <class ValueType, class Order>
struct Sort {
  size_t Quantity;

  void run(benchmark::State& state) const {
    runOpOnCopies<ValueType>(
        state, Quantity, Order(), BatchSize::CountElements,
        [](auto& Copy) { std::sort(Copy.begin(), Copy.end()); });
  }

  bool skip() const { return Order() == ::Order::Heap; }

  std::string name() const {
    return "BM_Sort" + ValueType::name() + Order::name() + "_" +
           std::to_string(Quantity);
  };
};
} // namespace

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  makeCartesianProductBenchmark<Sort, AllValueTypes, AllOrders>(Quantities);
  benchmark::RunSpecifiedBenchmarks();
}
