// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sample_vector.h"

#include <ostream>
#include <string_view>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/debug/crash_logging.h"
#include "base/debug/leak_annotations.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_span.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"

// This SampleVector makes use of the single-sample embedded in the base
// HistogramSamples class. If the count is non-zero then there is guaranteed
// (within the bounds of "eventual consistency") to be no allocated external
// storage. Once the full counts storage is allocated, the single-sample must
// be extracted and disabled.

namespace base {

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

namespace {

// An iterator for sample vectors.
template <typename T>
class IteratorTemplate : public SampleCountIterator {
 public:
  IteratorTemplate(base::span<T> counts, const BucketRanges* bucket_ranges)
      : counts_(counts), bucket_ranges_(bucket_ranges) {
    SkipEmptyBuckets();
  }

  ~IteratorTemplate() override;

  // SampleCountIterator:
  bool Done() const override { return index_ >= counts_.size(); }
  void Next() override {
    DCHECK(!Done());
    index_++;
    SkipEmptyBuckets();
  }
  void Get(HistogramBase::Sample* min,
           int64_t* max,
           HistogramBase::Count* count) override;

  // SampleVector uses predefined buckets, so iterator can return bucket index.
  bool GetBucketIndex(size_t* index) const override {
    DCHECK(!Done());
    if (index != nullptr) {
      *index = index_;
    }
    return true;
  }

 private:
  void SkipEmptyBuckets() {
    if (Done()) {
      return;
    }

    while (index_ < counts_.size()) {
      if (subtle::NoBarrier_Load(&counts_[index_]) != 0) {
        return;
      }
      index_++;
    }
  }

  raw_span<T> counts_;
  raw_ptr<const BucketRanges> bucket_ranges_;
  size_t index_ = 0;
};

using SampleVectorIterator = IteratorTemplate<const HistogramBase::AtomicCount>;

template <>
SampleVectorIterator::~IteratorTemplate() = default;

// Get() for an iterator of a SampleVector.
template <>
void SampleVectorIterator::Get(HistogramBase::Sample* min,
                               int64_t* max,
                               HistogramBase::Count* count) {
  DCHECK(!Done());
  *min = bucket_ranges_->range(index_);
  *max = strict_cast<int64_t>(bucket_ranges_->range(index_ + 1));
  *count = subtle::NoBarrier_Load(&counts_[index_]);
}

using ExtractingSampleVectorIterator =
    IteratorTemplate<HistogramBase::AtomicCount>;

template <>
ExtractingSampleVectorIterator::~IteratorTemplate() {
  // Ensure that the user has consumed all the samples in order to ensure no
  // samples are lost.
  DCHECK(Done());
}

// Get() for an extracting iterator of a SampleVector.
template <>
void ExtractingSampleVectorIterator::Get(HistogramBase::Sample* min,
                                         int64_t* max,
                                         HistogramBase::Count* count) {
  DCHECK(!Done());
  *min = bucket_ranges_->range(index_);
  *max = strict_cast<int64_t>(bucket_ranges_->range(index_ + 1));
  *count = subtle::NoBarrier_AtomicExchange(&counts_[index_], 0);
}

}  // namespace

SampleVectorBase::SampleVectorBase(uint64_t id,
                                   Metadata* meta,
                                   const BucketRanges* bucket_ranges)
    : HistogramSamples(id, meta),
      bucket_ranges_(bucket_ranges),
      counts_size_(bucket_ranges_->bucket_count()) {
  CHECK_GE(counts_size_, 1u);
}

SampleVectorBase::SampleVectorBase(uint64_t id,
                                   std::unique_ptr<Metadata> meta,
                                   const BucketRanges* bucket_ranges)
    : HistogramSamples(id, std::move(meta)),
      bucket_ranges_(bucket_ranges),
      counts_size_(bucket_ranges_->bucket_count()) {
  CHECK_GE(counts_size_, 1u);
}

SampleVectorBase::~SampleVectorBase() = default;

void SampleVectorBase::Accumulate(Sample value, Count count) {
  const size_t bucket_index = GetBucketIndex(value);

  // Handle the single-sample case.
  if (!counts().has_value()) {
    // Try to accumulate the parameters into the single-count entry.
    if (AccumulateSingleSample(value, count, bucket_index)) {
      // A race condition could lead to a new single-sample being accumulated
      // above just after another thread executed the MountCountsStorage below.
      // Since it is mounted, it could be mounted elsewhere and have values
      // written to it. It's not allowed to have both a single-sample and
      // entries in the counts array so move the single-sample.
      if (counts().has_value()) {
        MoveSingleSampleToCounts();
      }
      return;
    }

    // Need real storage to store both what was in the single-sample plus the
    // parameter information.
    MountCountsStorageAndMoveSingleSample();
  }

  // Handle the multi-sample case.
  Count new_bucket_count =
      subtle::NoBarrier_AtomicIncrement(&counts_at(bucket_index), count);
  IncreaseSumAndCount(strict_cast<int64_t>(count) * value, count);

  // TODO(bcwhite) Remove after crbug.com/682680.
  Count old_bucket_count = new_bucket_count - count;
  bool record_negative_sample =
      (new_bucket_count >= 0) != (old_bucket_count >= 0) && count > 0;
  if (UNLIKELY(record_negative_sample)) {
    RecordNegativeSample(SAMPLES_ACCUMULATE_OVERFLOW, count);
  }
}

Count SampleVectorBase::GetCount(Sample value) const {
  return GetCountAtIndex(GetBucketIndex(value));
}

Count SampleVectorBase::TotalCount() const {
  // Handle the single-sample case.
  SingleSample sample = single_sample().Load();
  if (sample.count != 0) {
    return sample.count;
  }

  // Handle the multi-sample case.
  if (counts().has_value() || MountExistingCountsStorage()) {
    Count count = 0;
    // TODO(danakj): In C++23 we can skip the `counts_span` lvalue and iterate
    // over `counts().value()` directly without creating a dangling reference.
    span<const HistogramBase::AtomicCount> counts_span = counts().value();
    for (const HistogramBase::AtomicCount& c : counts_span) {
      count += subtle::NoBarrier_Load(&c);
    }
    return count;
  }

  // And the no-value case.
  return 0;
}

Count SampleVectorBase::GetCountAtIndex(size_t bucket_index) const {
  DCHECK(bucket_index < counts_size());

  // Handle the single-sample case.
  SingleSample sample = single_sample().Load();
  if (sample.count != 0) {
    return sample.bucket == bucket_index ? sample.count : 0;
  }

  // Handle the multi-sample case.
  if (counts().has_value() || MountExistingCountsStorage()) {
    return subtle::NoBarrier_Load(&counts_at(bucket_index));
  }

  // And the no-value case.
  return 0;
}

std::unique_ptr<SampleCountIterator> SampleVectorBase::Iterator() const {
  // Handle the single-sample case.
  SingleSample sample = single_sample().Load();
  if (sample.count != 0) {
    static_assert(std::is_unsigned<decltype(SingleSample::bucket)>::value);
    if (sample.bucket >= bucket_ranges_->bucket_count()) {
      // Return an empty iterator if the specified bucket is invalid (e.g. due
      // to corruption). If a different sample is eventually emitted, we will
      // move from SingleSample to a counts storage, and that time, we will
      // discard this invalid sample (see MoveSingleSampleToCounts()).
      return std::make_unique<SampleVectorIterator>(
          base::span<const HistogramBase::AtomicCount>(), bucket_ranges_);
    }

    return std::make_unique<SingleSampleIterator>(
        bucket_ranges_->range(sample.bucket),
        bucket_ranges_->range(sample.bucket + 1), sample.count, sample.bucket,
        /*value_was_extracted=*/false);
  }

  // Handle the multi-sample case.
  if (counts().has_value() || MountExistingCountsStorage()) {
    return std::make_unique<SampleVectorIterator>(*counts(), bucket_ranges_);
  }

  // And the no-value case.
  return std::make_unique<SampleVectorIterator>(
      base::span<const HistogramBase::AtomicCount>(), bucket_ranges_);
}

std::unique_ptr<SampleCountIterator> SampleVectorBase::ExtractingIterator() {
  // Handle the single-sample case.
  SingleSample sample = single_sample().Extract();
  if (sample.count != 0) {
    static_assert(std::is_unsigned<decltype(SingleSample::bucket)>::value);
    if (sample.bucket >= bucket_ranges_->bucket_count()) {
      // Return an empty iterator if the specified bucket is invalid (e.g. due
      // to corruption). Note that we've already removed the sample from the
      // underlying data, so this invalid sample is discarded.
      return std::make_unique<ExtractingSampleVectorIterator>(
          base::span<HistogramBase::AtomicCount>(), bucket_ranges_);
    }

    // Note that we have already extracted the samples (i.e., reset the
    // underlying data back to 0 samples), even before the iterator has been
    // used. This means that the caller needs to ensure that this value is
    // eventually consumed, otherwise the sample is lost. There is no iterator
    // that simply points to the underlying SingleSample and extracts its value
    // on-demand because there are tricky edge cases when the SingleSample is
    // disabled between the creation of the iterator and the actual call to
    // Get() (for example, due to histogram changing to use a vector to store
    // its samples).
    return std::make_unique<SingleSampleIterator>(
        bucket_ranges_->range(sample.bucket),
        bucket_ranges_->range(sample.bucket + 1), sample.count, sample.bucket,
        /*value_was_extracted=*/true);
  }

  // Handle the multi-sample case.
  if (counts().has_value() || MountExistingCountsStorage()) {
    return std::make_unique<ExtractingSampleVectorIterator>(*counts(),
                                                            bucket_ranges_);
  }

  // And the no-value case.
  return std::make_unique<ExtractingSampleVectorIterator>(
      base::span<HistogramBase::AtomicCount>(), bucket_ranges_);
}

bool SampleVectorBase::AddSubtractImpl(SampleCountIterator* iter,
                                       HistogramSamples::Operator op) {
  // Stop now if there's nothing to do.
  if (iter->Done()) {
    return true;
  }

  // Get the first value and its index.
  HistogramBase::Sample min;
  int64_t max;
  HistogramBase::Count count;
  iter->Get(&min, &max, &count);
  size_t dest_index = GetBucketIndex(min);

  // The destination must be a superset of the source meaning that though the
  // incoming ranges will find an exact match, the incoming bucket-index, if
  // it exists, may be offset from the destination bucket-index. Calculate
  // that offset of the passed iterator; there are are no overflow checks
  // because 2's compliment math will work it out in the end.
  //
  // Because GetBucketIndex() always returns the same true or false result for
  // a given iterator object, |index_offset| is either set here and used below,
  // or never set and never used. The compiler doesn't know this, though, which
  // is why it's necessary to initialize it to something.
  size_t index_offset = 0;
  size_t iter_index;
  if (iter->GetBucketIndex(&iter_index)) {
    index_offset = dest_index - iter_index;
  }
  if (dest_index >= counts_size()) {
    return false;
  }

  // Post-increment. Information about the current sample is not available
  // after this point.
  iter->Next();

  // Single-value storage is possible if there is no counts storage and the
  // retrieved entry is the only one in the iterator.
  if (!counts().has_value()) {
    if (iter->Done()) {
      // Don't call AccumulateSingleSample because that updates sum and count
      // which was already done by the caller of this method.
      if (single_sample().Accumulate(
              dest_index, op == HistogramSamples::ADD ? count : -count)) {
        // Handle race-condition that mounted counts storage between above and
        // here.
        if (counts().has_value()) {
          MoveSingleSampleToCounts();
        }
        return true;
      }
    }

    // The counts storage will be needed to hold the multiple incoming values.
    MountCountsStorageAndMoveSingleSample();
  }

  // Go through the iterator and add the counts into correct bucket.
  while (true) {
    // Ensure that the sample's min/max match the ranges min/max.
    if (min != bucket_ranges_->range(dest_index) ||
        max != bucket_ranges_->range(dest_index + 1)) {
#if !BUILDFLAG(IS_NACL)
      // TODO(crbug/1432981): Remove these. They are used to investigate
      // unexpected failures.
      SCOPED_CRASH_KEY_NUMBER("SampleVector", "min", min);
      SCOPED_CRASH_KEY_NUMBER("SampleVector", "max", max);
      SCOPED_CRASH_KEY_NUMBER("SampleVector", "range_min",
                              bucket_ranges_->range(dest_index));
      SCOPED_CRASH_KEY_NUMBER("SampleVector", "range_max",
                              bucket_ranges_->range(dest_index + 1));
#endif  // !BUILDFLAG(IS_NACL)
      NOTREACHED() << "sample=" << min << "," << max
                   << "; range=" << bucket_ranges_->range(dest_index) << ","
                   << bucket_ranges_->range(dest_index + 1);
      return false;
    }

    // Sample's bucket matches exactly. Adjust count.
    subtle::NoBarrier_AtomicIncrement(
        &counts_at(dest_index), op == HistogramSamples::ADD ? count : -count);

    // Advance to the next iterable sample. See comments above for how
    // everything works.
    if (iter->Done()) {
      return true;
    }
    iter->Get(&min, &max, &count);
    if (iter->GetBucketIndex(&iter_index)) {
      // Destination bucket is a known offset from the source bucket.
      dest_index = iter_index + index_offset;
    } else {
      // Destination bucket has to be determined anew each time.
      dest_index = GetBucketIndex(min);
    }
    if (dest_index >= counts_size()) {
      return false;
    }
    iter->Next();
  }
}

// Uses simple binary search or calculates the index directly if it's an "exact"
// linear histogram. This is very general, but there are better approaches if we
// knew that the buckets were linearly distributed.
size_t SampleVectorBase::GetBucketIndex(Sample value) const {
  size_t bucket_count = bucket_ranges_->bucket_count();
  CHECK_GE(value, bucket_ranges_->range(0));
  CHECK_LT(value, bucket_ranges_->range(bucket_count));

  // For "exact" linear histograms, e.g. bucket_count = maximum + 1, their
  // minimum is 1 and bucket sizes are 1. Thus, we don't need to binary search
  // the bucket index. The bucket index for bucket |value| is just the |value|.
  Sample maximum = bucket_ranges_->range(bucket_count - 1);
  if (maximum == static_cast<Sample>(bucket_count - 1)) {
    // |value| is in the underflow bucket.
    if (value < 1) {
      return 0;
    }
    // |value| is in the overflow bucket.
    if (value > maximum) {
      return bucket_count - 1;
    }
    return static_cast<size_t>(value);
  }

  size_t under = 0;
  size_t over = bucket_count;
  size_t mid;
  do {
    DCHECK_GE(over, under);
    mid = under + (over - under) / 2;
    if (mid == under) {
      break;
    }
    if (bucket_ranges_->range(mid) <= value) {
      under = mid;
    } else {
      over = mid;
    }
  } while (true);

  DCHECK_LE(bucket_ranges_->range(mid), value);
  CHECK_GT(bucket_ranges_->range(mid + 1), value);
  return mid;
}

void SampleVectorBase::MoveSingleSampleToCounts() {
  DCHECK(counts().has_value());

  // Disable the single-sample since there is now counts storage for the data.
  SingleSample sample = single_sample().ExtractAndDisable();

  // Stop here if there is no "count" as trying to find the bucket index of
  // an invalid (including zero) "value" will crash.
  if (sample.count == 0) {
    return;
  }

  // Stop here if the sample bucket would be out of range for the AtomicCount
  // array.
  if (sample.bucket >= counts_size()) {
    return;
  }

  // Move the value into storage. Sum and redundant-count already account
  // for this entry so no need to call IncreaseSumAndCount().
  subtle::NoBarrier_AtomicIncrement(&counts_at(sample.bucket), sample.count);
}

void SampleVectorBase::MountCountsStorageAndMoveSingleSample() {
  // There are many SampleVector objects and the lock is needed very
  // infrequently (just when advancing from single-sample to multi-sample) so
  // define a single, global lock that all can use. This lock only prevents
  // concurrent entry into the code below; access and updates to |counts_data_|
  // still requires atomic operations.
  static LazyInstance<Lock>::Leaky counts_lock = LAZY_INSTANCE_INITIALIZER;
  if (counts_data_.load(std::memory_order_relaxed) == nullptr) {
    AutoLock lock(counts_lock.Get());
    if (counts_data_.load(std::memory_order_relaxed) == nullptr) {
      // Create the actual counts storage while the above lock is acquired.
      span<HistogramBase::Count> counts = CreateCountsStorageWhileLocked();
      // Point |counts()| to the newly created storage. This is done while
      // locked to prevent possible concurrent calls to CreateCountsStorage
      // but, between that call and here, other threads could notice the
      // existence of the storage and race with this to set_counts(). That's
      // okay because (a) it's atomic and (b) it always writes the same value.
      set_counts(counts);
    }
  }

  // Move any single-sample into the newly mounted storage.
  MoveSingleSampleToCounts();
}

SampleVector::SampleVector(const BucketRanges* bucket_ranges)
    : SampleVector(0, bucket_ranges) {}

SampleVector::SampleVector(uint64_t id, const BucketRanges* bucket_ranges)
    : SampleVectorBase(id, std::make_unique<LocalMetadata>(), bucket_ranges) {}

SampleVector::~SampleVector() = default;

bool SampleVector::IsDefinitelyEmpty() const {
  // If we are still using SingleSample, and it has a count of 0, then |this|
  // has no samples. If we are not using SingleSample, always return false, even
  // though it is possible that |this| has no samples (e.g. we are using a
  // counts array and all the bucket counts are 0). If we are wrong, this will
  // just make the caller perform some extra work thinking that |this| is
  // non-empty.
  AtomicSingleSample sample = single_sample();
  return HistogramSamples::IsDefinitelyEmpty() && !sample.IsDisabled() &&
         sample.Load().count == 0;
}

bool SampleVector::MountExistingCountsStorage() const {
  // There is never any existing storage other than what is already in use.
  return counts().has_value();
}

std::string SampleVector::GetAsciiHeader(std::string_view histogram_name,
                                         int32_t flags) const {
  Count sample_count = TotalCount();
  std::string output;
  StrAppend(&output, {"Histogram: ", histogram_name, " recorded ",
                      NumberToString(sample_count), " samples"});
  if (sample_count == 0) {
    DCHECK_EQ(sum(), 0);
  } else {
    double mean = static_cast<float>(sum()) / sample_count;
    StringAppendF(&output, ", mean = %.1f", mean);
  }
  if (flags) {
    StringAppendF(&output, " (flags = 0x%x)", flags);
  }
  return output;
}

std::string SampleVector::GetAsciiBody() const {
  Count sample_count = TotalCount();

  // Prepare to normalize graphical rendering of bucket contents.
  double max_size = 0;
  double scaling_factor = 1;
  max_size = GetPeakBucketSize();
  // Scale histogram bucket counts to take at most 72 characters.
  // Note: Keep in sync w/ kLineLength histogram_samples.cc
  const double kLineLength = 72;
  if (max_size > kLineLength) {
    scaling_factor = kLineLength / max_size;
  }

  // Calculate largest print width needed for any of our bucket range displays.
  size_t print_width = 1;
  for (uint32_t i = 0; i < bucket_count(); ++i) {
    if (GetCountAtIndex(i)) {
      size_t width =
          GetSimpleAsciiBucketRange(bucket_ranges()->range(i)).size() + 1;
      if (width > print_width) {
        print_width = width;
      }
    }
  }

  int64_t remaining = sample_count;
  int64_t past = 0;
  std::string output;
  // Output the actual histogram graph.
  for (uint32_t i = 0; i < bucket_count(); ++i) {
    Count current = GetCountAtIndex(i);
    remaining -= current;
    std::string range = GetSimpleAsciiBucketRange(bucket_ranges()->range(i));
    output.append(range);
    for (size_t j = 0; range.size() + j < print_width + 1; ++j) {
      output.push_back(' ');
    }
    if (0 == current && i < bucket_count() - 1 && 0 == GetCountAtIndex(i + 1)) {
      while (i < bucket_count() - 1 && 0 == GetCountAtIndex(i + 1)) {
        ++i;
      }
      output.append("... \n");
      continue;  // No reason to plot emptiness.
    }
    Count current_size = round(current * scaling_factor);
    WriteAsciiBucketGraph(current_size, kLineLength, &output);
    WriteAsciiBucketContext(past, current, remaining, i, &output);
    output.append("\n");
    past += current;
  }
  DCHECK_EQ(sample_count, past);
  return output;
}

double SampleVector::GetPeakBucketSize() const {
  Count max = 0;
  for (uint32_t i = 0; i < bucket_count(); ++i) {
    Count current = GetCountAtIndex(i);
    if (current > max) {
      max = current;
    }
  }
  return max;
}

void SampleVector::WriteAsciiBucketContext(int64_t past,
                                           Count current,
                                           int64_t remaining,
                                           uint32_t current_bucket_index,
                                           std::string* output) const {
  double scaled_sum = (past + current + remaining) / 100.0;
  WriteAsciiBucketValue(current, scaled_sum, output);
  if (0 < current_bucket_index) {
    double percentage = past / scaled_sum;
    StringAppendF(output, " {%3.1f%%}", percentage);
  }
}

span<HistogramBase::AtomicCount>
SampleVector::CreateCountsStorageWhileLocked() {
  local_counts_.resize(counts_size());
  return local_counts_;
}

PersistentSampleVector::PersistentSampleVector(
    uint64_t id,
    const BucketRanges* bucket_ranges,
    Metadata* meta,
    const DelayedPersistentAllocation& counts)
    : SampleVectorBase(id, meta, bucket_ranges), persistent_counts_(counts) {
  // Only mount the full storage if the single-sample has been disabled.
  // Otherwise, it is possible for this object instance to start using (empty)
  // storage that was created incidentally while another instance continues to
  // update to the single sample. This "incidental creation" can happen because
  // the memory is a DelayedPersistentAllocation which allows multiple memory
  // blocks within it and applies an all-or-nothing approach to the allocation.
  // Thus, a request elsewhere for one of the _other_ blocks would make _this_
  // block available even though nothing has explicitly requested it.
  //
  // Note that it's not possible for the ctor to mount existing storage and
  // move any single-sample to it because sometimes the persistent memory is
  // read-only. Only non-const methods (which assume that memory is read/write)
  // can do that.
  if (single_sample().IsDisabled()) {
    bool success = MountExistingCountsStorage();
    DCHECK(success);
  }
}

PersistentSampleVector::~PersistentSampleVector() = default;

bool PersistentSampleVector::IsDefinitelyEmpty() const {
  // Not implemented.
  NOTREACHED();

  // Always return false. If we are wrong, this will just make the caller
  // perform some extra work thinking that |this| is non-empty.
  return false;
}

bool PersistentSampleVector::MountExistingCountsStorage() const {
  // There is no early exit if counts is not yet mounted because, given that
  // this is a virtual function, it's more efficient to do that at the call-
  // site. There is no danger, however, should this get called anyway (perhaps
  // because of a race condition) because at worst the `counts_data_` and
  // `counts_size_` members would be over-written (in an atomic manner)
  // with the exact same values.

  if (!persistent_counts_.reference()) {
    return false;  // Nothing to mount.
  }

  // Mount the counts array in position. This shouldn't fail but can if the
  // data is corrupt or incomplete.
  span<HistogramBase::AtomicCount> mem =
      persistent_counts_.Get<HistogramBase::AtomicCount>();
  if (mem.empty()) {
    return false;
  }
  // Uses a span that only covers the counts the SampleVector should have
  // access to, which can be a subset of the entire persistent allocation.
  set_counts(mem.first(counts_size()));
  return true;
}

span<HistogramBase::AtomicCount>
PersistentSampleVector::CreateCountsStorageWhileLocked() {
  span<HistogramBase::AtomicCount> mem =
      persistent_counts_.Get<HistogramBase::AtomicCount>();
  if (mem.empty()) {
    // The above shouldn't fail but can if Bad Things(tm) are occurring in
    // the persistent allocator. Crashing isn't a good option so instead
    // just allocate something from the heap that we will leak and return that.
    // There will be no sharing or persistence but worse things are already
    // happening.
    auto array = HeapArray<HistogramBase::AtomicCount>::WithSize(counts_size());
    ANNOTATE_LEAKING_OBJECT_PTR(array.data());
    return std::move(array).leak();
  }

  // Returns a span that only covers the counts the SampleVector should have
  // access to, which can be a subset of the entire persistent allocation.
  return mem.first(counts_size());
}

}  // namespace base
