// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/default_delayed_task_handle_delegate.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"

namespace base {

namespace {

ThreadLocalPointer<SequencedTaskRunner::CurrentDefaultHandle>&
CurrentDefaultHandleTls() {
  static NoDestructor<
      ThreadLocalPointer<SequencedTaskRunner::CurrentDefaultHandle>>
      instance;
  return *instance;
}

}  // namespace

bool SequencedTaskRunner::PostNonNestableTask(const Location& from_here,
                                              OnceClosure task) {
  return PostNonNestableDelayedTask(from_here, std::move(task),
                                    base::TimeDelta());
}

DelayedTaskHandle SequencedTaskRunner::PostCancelableDelayedTask(
    subtle::PostDelayedTaskPassKey,
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  auto delayed_task_handle_delegate =
      std::make_unique<DefaultDelayedTaskHandleDelegate>();

  task = delayed_task_handle_delegate->BindCallback(std::move(task));

  DelayedTaskHandle delayed_task_handle(
      std::move(delayed_task_handle_delegate));

  // If the task fails to be posted, the handle will automatically be
  // invalidated upon destruction of the callback object.
  if (!PostDelayedTask(from_here, std::move(task), delay))
    DCHECK(!delayed_task_handle.IsValid());

  return delayed_task_handle;
}

DelayedTaskHandle SequencedTaskRunner::PostCancelableDelayedTaskAt(
    subtle::PostDelayedTaskPassKey pass_key,
    const Location& from_here,
    OnceClosure task,
    TimeTicks delayed_run_time,
    subtle::DelayPolicy deadline_policy) {
  auto delayed_task_handle_delegate =
      std::make_unique<DefaultDelayedTaskHandleDelegate>();

  task = delayed_task_handle_delegate->BindCallback(std::move(task));

  DelayedTaskHandle delayed_task_handle(
      std::move(delayed_task_handle_delegate));

  if (!PostDelayedTaskAt(pass_key, from_here, std::move(task), delayed_run_time,
                         deadline_policy)) {
    DCHECK(!delayed_task_handle.IsValid());
  }
  return delayed_task_handle;
}

bool SequencedTaskRunner::PostDelayedTaskAt(
    subtle::PostDelayedTaskPassKey,
    const Location& from_here,
    OnceClosure task,
    TimeTicks delayed_run_time,
    subtle::DelayPolicy deadline_policy) {
  return PostDelayedTask(from_here, std::move(task),
                         delayed_run_time.is_null()
                             ? base::TimeDelta()
                             : delayed_run_time - TimeTicks::Now());
}

// static
const scoped_refptr<SequencedTaskRunner>&
SequencedTaskRunner::GetCurrentDefault() {
  const CurrentDefaultHandle* current_default = CurrentDefaultHandleTls().Get();
  CHECK(current_default)
      << "Error: This caller requires a sequenced context (i.e. the current "
         "task needs to run from a SequencedTaskRunner). If you're in a test "
         "refer to //docs/threading_and_tasks_testing.md.";
  return current_default->task_runner_;
}

// static
bool SequencedTaskRunner::HasCurrentDefault() {
  return !!CurrentDefaultHandleTls().Get();
}

SequencedTaskRunner::CurrentDefaultHandle::CurrentDefaultHandle(
    scoped_refptr<SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!SequencedTaskRunner::HasCurrentDefault());
  CurrentDefaultHandleTls().Set(this);
}

SequencedTaskRunner::CurrentDefaultHandle::~CurrentDefaultHandle() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(CurrentDefaultHandleTls().Get(), this);
  CurrentDefaultHandleTls().Set(nullptr);
}

bool SequencedTaskRunner::DeleteOrReleaseSoonInternal(
    const Location& from_here,
    void (*deleter)(const void*),
    const void* object) {
  return PostNonNestableTask(from_here, BindOnce(deleter, object));
}

OnTaskRunnerDeleter::OnTaskRunnerDeleter(
    scoped_refptr<SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
}

OnTaskRunnerDeleter::~OnTaskRunnerDeleter() = default;

OnTaskRunnerDeleter::OnTaskRunnerDeleter(OnTaskRunnerDeleter&&) = default;

OnTaskRunnerDeleter& OnTaskRunnerDeleter::operator=(
    OnTaskRunnerDeleter&&) = default;

}  // namespace base
