// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_FUTURE_H_
#define BASE_TEST_TEST_FUTURE_H_

#include <memory>
#include <string>
#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/test_future_internal.h"
#include "base/thread_annotations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::test {

// Helper class to test code that returns its result(s) asynchronously through a
// callback:
//
//    - Pass the callback provided by `GetCallback()` to the code under test.
//    - Wait for the callback to be invoked by calling `Wait(),` or `Get()` to
//      access the value(s) passed to the callback.
//
//   Example usage:
//
//     TEST_F(MyTestFixture, MyTest) {
//       TestFuture<ResultType> future;
//
//       object_under_test.DoSomethingAsync(future.GetCallback());
//
//       const ResultType& actual_result = future.Get();
//
//       // When you come here, DoSomethingAsync has finished and
//       // `actual_result` contains the result passed to the callback.
//     }
//
//   Example using `Wait()`:
//
//     TEST_F(MyTestFixture, MyWaitTest) {
//       TestFuture<ResultType> future;
//
//       object_under_test.DoSomethingAsync(future.GetCallback());
//
//       // Optional. The Get() call below will also wait until the value
//       // arrives, but this explicit call to Wait() can be useful if you want
//       // to add extra information.
//       ASSERT_TRUE(future.Wait()) << "Detailed error message";
//
//       const ResultType& actual_result = future.Get();
//     }
//
// `TestFuture` supports both single- and multiple-argument callbacks.
// `TestFuture` provides both index and type based accessors for multi-argument
// callbacks. `Get()` and `Take()` return tuples for multi-argument callbacks.
//
//   TestFuture<int, std::string> future;
//   future.Get<0>();   // Reads the first argument
//   future.Get<int>(); // Also reads the first argument
//   future.Get();      // Returns a `const std::tuple<int, std::string>&`
//
//   Example for a multi-argument callback:
//
//     TEST_F(MyTestFixture, MyTest) {
//       TestFuture<int, std::string> future;
//
//       object_under_test.DoSomethingAsync(future.GetCallback());
//
//       // You can use type based accessors:
//       int first_argument = future.Get<int>();
//       const std::string& second_argument = future.Get<std::string>();
//
//       // or index based accessors:
//       int first_argument = future.Get<0>();
//       const std::string& second_argument = future.Get<1>();
//     }
//
// You can also satisfy a `TestFuture` by calling `SetValue()` from the sequence
// on which the `TestFuture` was created. This is mostly useful when
// implementing an observer:
//
//     class MyTestObserver: public MyObserver {
//       public:
//         // `MyObserver` implementation:
//         void ObserveAnInt(int value) override {
//           future_.SetValue(value);
//         }
//
//         int Wait() { return future_.Take(); }
//
//      private:
//        TestFuture<int> future_;
//     };
//
//     TEST_F(MyTestFixture, MyTest) {
//       MyTestObserver observer;
//
//       object_under_test.DoSomethingAsync(observer);
//
//       int value_passed_to_observer = observer.Wait();
//     };
//
// `GetRepeatingCallback()` allows you to use a single `TestFuture` in code
// that invokes the callback multiple times.
// Your test must take care to consume each value before the next value
// arrives. You can consume the value by calling either `Take()` or `Clear()`.
//
//   Example for reusing a `TestFuture`:
//
//     TEST_F(MyTestFixture, MyReuseTest) {
//       TestFuture<std::string> future;
//
//       object_under_test.InstallCallback(future.GetRepeatingCallback());
//
//       object_under_test.DoSomething();
//       EXPECT_EQ(future.Take(), "expected-first-value");
//       // Because we used `Take()` the test future is ready for reuse.
//
//       object_under_test.DoSomethingElse();
//       EXPECT_EQ(future.Take(), "expected-second-value");
//     }
//
//   Example for reusing  a `TestFuture` using `Get()` + `Clear()`:
//
//     TEST_F(MyTestFixture, MyReuseTest) {
//       TestFuture<std::string, int> future;
//
//       object_under_test.InstallCallback(future.GetRepeatingCallback());
//
//       object_under_test.DoSomething();
//
//       EXPECT_EQ(future.Get<std::string>(), "expected-first-value");
//       EXPECT_EQ(future.Get<int>(), 5);
//       // Because we used `Get()`, the test future is not ready for reuse,
//       //so we need an explicit `Clear()` call.
//       future.Clear();
//
//       object_under_test.DoSomethingElse();
//       EXPECT_EQ(future.Get<std::string>(), "expected-second-value");
//       EXPECT_EQ(future.Get<int>(), 2);
//     }
//
// Finally, `TestFuture` also supports no-args callbacks:
//
//   Example for no-args callbacks:
//
//     TEST_F(MyTestFixture, MyTest) {
//       TestFuture<void> signal;
//
//       object_under_test.DoSomethingAsync(signal.GetCallback());
//
//       EXPECT_TRUE(signal.Wait());
//       // When you come here you know the callback was invoked and the async
//       // code is ready.
//     }
//
// All access to this class and its callbacks must be made from the sequence on
// which the `TestFuture` was constructed.
//
template <typename... Types>
class TestFuture {
 public:
  using TupleType = std::tuple<std::decay_t<Types>...>;

  static_assert(std::tuple_size<TupleType>::value > 0,
                "Don't use TestFuture<> but use TestFuture<void> instead");

  TestFuture() = default;
  TestFuture(const TestFuture&) = delete;
  TestFuture& operator=(const TestFuture&) = delete;
  ~TestFuture() = default;

  // Waits for the value to arrive.
  //
  // Returns true if the value arrived, or false if a timeout happens.
  //
  // Directly calling Wait() is not required as Get()/Take() will also wait for
  // the value to arrive, however you can use a direct call to Wait() to
  // improve the error reported:
  //
  //   ASSERT_TRUE(queue.Wait()) << "Detailed error message";
  //
  [[nodiscard]] bool Wait() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (values_) {
      return true;
    }

    run_loop_->Run();

    return IsReady();
  }

  // Returns true if the value has arrived.
  bool IsReady() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return values_.has_value();
  }

  // Waits for the value to arrive, and returns the I-th value.
  //
  // Will DCHECK if a timeout happens.
  //
  // Example usage:
  //
  //   TestFuture<int, std::string> future;
  //   int first = future.Get<0>();
  //   std::string second = future.Get<1>();
  //
  template <std::size_t I,
            typename T = TupleType,
            internal::EnableIfOneOrMoreValues<T> = true>
  const auto& Get() {
    return std::get<I>(GetTuple());
  }

  // Waits for the value to arrive, and returns the value with the given type.
  //
  // Will DCHECK if a timeout happens.
  //
  // Example usage:
  //
  //   TestFuture<int, std::string> future;
  //   int first = future.Get<int>();
  //   std::string second = future.Get<std::string>();
  //
  template <typename Type>
  const auto& Get() {
    return std::get<Type>(GetTuple());
  }

  // Returns a callback that when invoked will store all the argument values,
  // and unblock any waiters.
  // Templated so you can specify how you need the arguments to be passed -
  // const, reference, .... Defaults to simply `Types...`.
  //
  // Example usage:
  //
  //   TestFuture<int, std::string> future;
  //
  //   // returns base::OnceCallback<void(int, std::string)>
  //   future.GetCallback();
  //
  //   // returns base::OnceCallback<void(int, const std::string&)>
  //   future.GetCallback<int, const std::string&>();
  //
  template <typename... CallbackArgumentsTypes>
  OnceCallback<void(CallbackArgumentsTypes...)> GetCallback() {
    return GetRepeatingCallback<CallbackArgumentsTypes...>();
  }

  OnceCallback<void(Types...)> GetCallback() { return GetCallback<Types...>(); }

  // Returns a repeating callback that when invoked will store all the argument
  // values, and unblock any waiters.
  //
  // You must take care that the stored value is consumed before the callback
  // is invoked a second time.
  // You can consume the value by calling either `Take()` or `Clear()`.
  //
  // Example usage:
  //
  //   TestFuture<std::string> future;
  //
  //   object_under_test.InstallCallback(future.GetRepeatingCallback());
  //
  //   object_under_test.DoSomething();
  //   EXPECT_EQ(future.Take(), "expected-first-value");
  //   // Because we used `Take()` the test future is ready for reuse.
  //
  //   object_under_test.DoSomethingElse();
  //   // We can also use `Get()` + `Clear()` to reuse the callback.
  //   EXPECT_EQ(future.Get(), "expected-second-value");
  //   future.Clear();
  //
  //   object_under_test.DoSomethingElse();
  //   EXPECT_EQ(future.Take(), "expected-third-value");
  //
  template <typename... CallbackArgumentsTypes>
  RepeatingCallback<void(CallbackArgumentsTypes...)> GetRepeatingCallback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return BindRepeating(
        [](WeakPtr<TestFuture<Types...>> future,
           CallbackArgumentsTypes... values) {
          if (future) {
            future->SetValue(std::forward<CallbackArgumentsTypes>(values)...);
          }
        },
        weak_ptr_factory_.GetWeakPtr());
  }

  RepeatingCallback<void(Types...)> GetRepeatingCallback() {
    return GetRepeatingCallback<Types...>();
  }

  // Sets the value of the future.
  // This will unblock any pending Wait() or Get() call.
  // This can only be called once.
  void SetValue(Types... values) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DCHECK(!values_.has_value())
        << "Overwriting previously stored value of the TestFuture."
           "If you expect this new value, be sure to first "
           "consume the stored value by calling `Take()` or `Clear()`";

    values_ = std::make_tuple(std::forward<Types>(values)...);
    run_loop_->Quit();
  }

  // Clears the future, allowing it to be reused and accept a new value.
  //
  // All outstanding callbacks issued through `GetCallback()` remain valid.
  void Clear() {
    if (IsReady()) {
      std::ignore = Take();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if the future holds a single value.
  //////////////////////////////////////////////////////////////////////////////

  // Waits for the value to arrive, and returns a reference to it.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfSingleValue<T> = true>
  [[nodiscard]] const auto& Get() {
    return std::get<0>(GetTuple());
  }

  // Waits for the value to arrive, and returns it.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfSingleValue<T> = true>
  [[nodiscard]] auto Take() {
    return std::get<0>(TakeTuple());
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if the future holds multiple values.
  //////////////////////////////////////////////////////////////////////////////

  // Waits for the values to arrive, and returns a tuple with the values.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfMultiValue<T> = true>
  [[nodiscard]] const TupleType& Get() {
    return GetTuple();
  }

  // Waits for the values to arrive, and moves a tuple with the values out.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfMultiValue<T> = true>
  [[nodiscard]] TupleType Take() {
    return TakeTuple();
  }

 private:
  [[nodiscard]] const TupleType& GetTuple() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool success = Wait();
    DCHECK(success) << "Waiting for value timed out.";
    return values_.value();
  }

  [[nodiscard]] TupleType TakeTuple() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool success = Wait();
    DCHECK(success) << "Waiting for value timed out.";

    run_loop_ = std::make_unique<RunLoop>();
    return std::exchange(values_, {}).value();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<RunLoop> run_loop_ GUARDED_BY_CONTEXT(sequence_checker_) =
      std::make_unique<RunLoop>();

  absl::optional<TupleType> values_ GUARDED_BY_CONTEXT(sequence_checker_);

  WeakPtrFactory<TestFuture<Types...>> weak_ptr_factory_{this};
};

// Specialization so you can use `TestFuture` to wait for a no-args callback.
//
// This specialization offers a subset of the methods provided on the base
// `TestFuture`, as there is no value to be returned.
template <>
class TestFuture<void> {
 public:
  // Waits until the callback or `SetValue()` is invoked.
  //
  // Fails your test if a timeout happens, but you can check the return value
  // to improve the error reported:
  //
  //   ASSERT_TRUE(future.Wait()) << "Detailed error message";
  [[nodiscard]] bool Wait() { return implementation_.Wait(); }

  // Waits until the callback or `SetValue()` is invoked.
  void Get() { std::ignore = implementation_.Get(); }

  // Returns true if the callback or `SetValue()` was invoked.
  bool IsReady() const { return implementation_.IsReady(); }

  // Returns a callback that when invoked will unblock any waiters.
  OnceCallback<void()> GetCallback() {
    return BindOnce(implementation_.GetCallback(), true);
  }

  // Returns a callback that when invoked will unblock any waiters.
  RepeatingCallback<void()> GetRepeatingCallback() {
    return BindRepeating(implementation_.GetRepeatingCallback(), true);
  }

  // Indicates this `TestFuture` is ready, and unblocks any waiters.
  void SetValue() { implementation_.SetValue(true); }

  // Clears the future, allowing it to be reused and accept a new value.
  //
  // All outstanding callbacks issued through `GetCallback()` remain valid.
  void Clear() { implementation_.Clear(); }

 private:
  TestFuture<bool> implementation_;
};

}  // namespace base::test

#endif  // BASE_TEST_TEST_FUTURE_H_
