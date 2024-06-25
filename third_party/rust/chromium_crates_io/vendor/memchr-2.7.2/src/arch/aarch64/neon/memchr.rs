/*!
This module defines 128-bit vector implementations of `memchr` and friends.

The main types in this module are [`One`], [`Two`] and [`Three`]. They are for
searching for one, two or three distinct bytes, respectively, in a haystack.
Each type also has corresponding double ended iterators. These searchers are
typically much faster than scalar routines accomplishing the same task.

The `One` searcher also provides a [`One::count`] routine for efficiently
counting the number of times a single byte occurs in a haystack. This is
useful, for example, for counting the number of lines in a haystack. This
routine exists because it is usually faster, especially with a high match
count, then using [`One::find`] repeatedly. ([`OneIter`] specializes its
`Iterator::count` implementation to use this routine.)

Only one, two and three bytes are supported because three bytes is about
the point where one sees diminishing returns. Beyond this point and it's
probably (but not necessarily) better to just use a simple `[bool; 256]` array
or similar. However, it depends mightily on the specific work-load and the
expected match frequency.
*/

use core::arch::aarch64::uint8x16_t;

use crate::{arch::generic::memchr as generic, ext::Pointer, vector::Vector};

/// Finds all occurrences of a single byte in a haystack.
#[derive(Clone, Copy, Debug)]
pub struct One(generic::One<uint8x16_t>);

impl One {
    /// Create a new searcher that finds occurrences of the needle byte given.
    ///
    /// This particular searcher is specialized to use neon vector instructions
    /// that typically make it quite fast.
    ///
    /// If neon is unavailable in the current environment, then `None` is
    /// returned.
    #[inline]
    pub fn new(needle: u8) -> Option<One> {
        if One::is_available() {
            // SAFETY: we check that neon is available above.
            unsafe { Some(One::new_unchecked(needle)) }
        } else {
            None
        }
    }

    /// Create a new finder specific to neon vectors and routines without
    /// checking that neon is available.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that it is safe to execute `neon` instructions
    /// in the current environment.
    ///
    /// Note that it is a common misconception that if one compiles for an
    /// `x86_64` target, then they therefore automatically have access to neon
    /// instructions. While this is almost always the case, it isn't true in
    /// 100% of cases.
    #[target_feature(enable = "neon")]
    #[inline]
    pub unsafe fn new_unchecked(needle: u8) -> One {
        One(generic::One::new(needle))
    }

    /// Returns true when this implementation is available in the current
    /// environment.
    ///
    /// When this is true, it is guaranteed that [`One::new`] will return
    /// a `Some` value. Similarly, when it is false, it is guaranteed that
    /// `One::new` will return a `None` value.
    ///
    /// Note also that for the lifetime of a single program, if this returns
    /// true then it will always return true.
    #[inline]
    pub fn is_available() -> bool {
        #[cfg(target_feature = "neon")]
        {
            true
        }
        #[cfg(not(target_feature = "neon"))]
        {
            false
        }
    }

    /// Return the first occurrence of one of the needle bytes in the given
    /// haystack. If no such occurrence exists, then `None` is returned.
    ///
    /// The occurrence is reported as an offset into `haystack`. Its maximum
    /// value is `haystack.len() - 1`.
    #[inline]
    pub fn find(&self, haystack: &[u8]) -> Option<usize> {
        // SAFETY: `find_raw` guarantees that if a pointer is returned, it
        // falls within the bounds of the start and end pointers.
        unsafe {
            generic::search_slice_with_raw(haystack, |s, e| {
                self.find_raw(s, e)
            })
        }
    }

    /// Return the last occurrence of one of the needle bytes in the given
    /// haystack. If no such occurrence exists, then `None` is returned.
    ///
    /// The occurrence is reported as an offset into `haystack`. Its maximum
    /// value is `haystack.len() - 1`.
    #[inline]
    pub fn rfind(&self, haystack: &[u8]) -> Option<usize> {
        // SAFETY: `rfind_raw` guarantees that if a pointer is returned, it
        // falls within the bounds of the start and end pointers.
        unsafe {
            generic::search_slice_with_raw(haystack, |s, e| {
                self.rfind_raw(s, e)
            })
        }
    }

    /// Counts all occurrences of this byte in the given haystack.
    #[inline]
    pub fn count(&self, haystack: &[u8]) -> usize {
        // SAFETY: All of our pointers are derived directly from a borrowed
        // slice, which is guaranteed to be valid.
        unsafe {
            let start = haystack.as_ptr();
            let end = start.add(haystack.len());
            self.count_raw(start, end)
        }
    }

    /// Like `find`, but accepts and returns raw pointers.
    ///
    /// When a match is found, the pointer returned is guaranteed to be
    /// `>= start` and `< end`.
    ///
    /// This routine is useful if you're already using raw pointers and would
    /// like to avoid converting back to a slice before executing a search.
    ///
    /// # Safety
    ///
    /// * Both `start` and `end` must be valid for reads.
    /// * Both `start` and `end` must point to an initialized value.
    /// * Both `start` and `end` must point to the same allocated object and
    /// must either be in bounds or at most one byte past the end of the
    /// allocated object.
    /// * Both `start` and `end` must be _derived from_ a pointer to the same
    /// object.
    /// * The distance between `start` and `end` must not overflow `isize`.
    /// * The distance being in bounds must not rely on "wrapping around" the
    /// address space.
    ///
    /// Note that callers may pass a pair of pointers such that `start >= end`.
    /// In that case, `None` will always be returned.
    #[inline]
    pub unsafe fn find_raw(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        if start >= end {
            return None;
        }
        if end.distance(start) < uint8x16_t::BYTES {
            // SAFETY: We require the caller to pass valid start/end pointers.
            return generic::fwd_byte_by_byte(start, end, |b| {
                b == self.0.needle1()
            });
        }
        // SAFETY: Building a `One` means it's safe to call 'neon' routines.
        // Also, we've checked that our haystack is big enough to run on the
        // vector routine. Pointer validity is caller's responsibility.
        self.find_raw_impl(start, end)
    }

    /// Like `rfind`, but accepts and returns raw pointers.
    ///
    /// When a match is found, the pointer returned is guaranteed to be
    /// `>= start` and `< end`.
    ///
    /// This routine is useful if you're already using raw pointers and would
    /// like to avoid converting back to a slice before executing a search.
    ///
    /// # Safety
    ///
    /// * Both `start` and `end` must be valid for reads.
    /// * Both `start` and `end` must point to an initialized value.
    /// * Both `start` and `end` must point to the same allocated object and
    /// must either be in bounds or at most one byte past the end of the
    /// allocated object.
    /// * Both `start` and `end` must be _derived from_ a pointer to the same
    /// object.
    /// * The distance between `start` and `end` must not overflow `isize`.
    /// * The distance being in bounds must not rely on "wrapping around" the
    /// address space.
    ///
    /// Note that callers may pass a pair of pointers such that `start >= end`.
    /// In that case, `None` will always be returned.
    #[inline]
    pub unsafe fn rfind_raw(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        if start >= end {
            return None;
        }
        if end.distance(start) < uint8x16_t::BYTES {
            // SAFETY: We require the caller to pass valid start/end pointers.
            return generic::rev_byte_by_byte(start, end, |b| {
                b == self.0.needle1()
            });
        }
        // SAFETY: Building a `One` means it's safe to call 'neon' routines.
        // Also, we've checked that our haystack is big enough to run on the
        // vector routine. Pointer validity is caller's responsibility.
        self.rfind_raw_impl(start, end)
    }

    /// Like `count`, but accepts and returns raw pointers.
    ///
    /// This routine is useful if you're already using raw pointers and would
    /// like to avoid converting back to a slice before executing a search.
    ///
    /// # Safety
    ///
    /// * Both `start` and `end` must be valid for reads.
    /// * Both `start` and `end` must point to an initialized value.
    /// * Both `start` and `end` must point to the same allocated object and
    /// must either be in bounds or at most one byte past the end of the
    /// allocated object.
    /// * Both `start` and `end` must be _derived from_ a pointer to the same
    /// object.
    /// * The distance between `start` and `end` must not overflow `isize`.
    /// * The distance being in bounds must not rely on "wrapping around" the
    /// address space.
    ///
    /// Note that callers may pass a pair of pointers such that `start >= end`.
    /// In that case, `None` will always be returned.
    #[inline]
    pub unsafe fn count_raw(&self, start: *const u8, end: *const u8) -> usize {
        if start >= end {
            return 0;
        }
        if end.distance(start) < uint8x16_t::BYTES {
            // SAFETY: We require the caller to pass valid start/end pointers.
            return generic::count_byte_by_byte(start, end, |b| {
                b == self.0.needle1()
            });
        }
        // SAFETY: Building a `One` means it's safe to call 'neon' routines.
        // Also, we've checked that our haystack is big enough to run on the
        // vector routine. Pointer validity is caller's responsibility.
        self.count_raw_impl(start, end)
    }

    /// Execute a search using neon vectors and routines.
    ///
    /// # Safety
    ///
    /// Same as [`One::find_raw`], except the distance between `start` and
    /// `end` must be at least the size of a neon vector (in bytes).
    ///
    /// (The target feature safety obligation is automatically fulfilled by
    /// virtue of being a method on `One`, which can only be constructed
    /// when it is safe to call `neon` routines.)
    #[target_feature(enable = "neon")]
    #[inline]
    unsafe fn find_raw_impl(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        self.0.find_raw(start, end)
    }

    /// Execute a search using neon vectors and routines.
    ///
    /// # Safety
    ///
    /// Same as [`One::rfind_raw`], except the distance between `start` and
    /// `end` must be at least the size of a neon vector (in bytes).
    ///
    /// (The target feature safety obligation is automatically fulfilled by
    /// virtue of being a method on `One`, which can only be constructed
    /// when it is safe to call `neon` routines.)
    #[target_feature(enable = "neon")]
    #[inline]
    unsafe fn rfind_raw_impl(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        self.0.rfind_raw(start, end)
    }

    /// Execute a count using neon vectors and routines.
    ///
    /// # Safety
    ///
    /// Same as [`One::count_raw`], except the distance between `start` and
    /// `end` must be at least the size of a neon vector (in bytes).
    ///
    /// (The target feature safety obligation is automatically fulfilled by
    /// virtue of being a method on `One`, which can only be constructed
    /// when it is safe to call `neon` routines.)
    #[target_feature(enable = "neon")]
    #[inline]
    unsafe fn count_raw_impl(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> usize {
        self.0.count_raw(start, end)
    }

    /// Returns an iterator over all occurrences of the needle byte in the
    /// given haystack.
    ///
    /// The iterator returned implements `DoubleEndedIterator`. This means it
    /// can also be used to find occurrences in reverse order.
    #[inline]
    pub fn iter<'a, 'h>(&'a self, haystack: &'h [u8]) -> OneIter<'a, 'h> {
        OneIter { searcher: self, it: generic::Iter::new(haystack) }
    }
}

/// An iterator over all occurrences of a single byte in a haystack.
///
/// This iterator implements `DoubleEndedIterator`, which means it can also be
/// used to find occurrences in reverse order.
///
/// This iterator is created by the [`One::iter`] method.
///
/// The lifetime parameters are as follows:
///
/// * `'a` refers to the lifetime of the underlying [`One`] searcher.
/// * `'h` refers to the lifetime of the haystack being searched.
#[derive(Clone, Debug)]
pub struct OneIter<'a, 'h> {
    searcher: &'a One,
    it: generic::Iter<'h>,
}

impl<'a, 'h> Iterator for OneIter<'a, 'h> {
    type Item = usize;

    #[inline]
    fn next(&mut self) -> Option<usize> {
        // SAFETY: We rely on the generic iterator to provide valid start
        // and end pointers, but we guarantee that any pointer returned by
        // 'find_raw' falls within the bounds of the start and end pointer.
        unsafe { self.it.next(|s, e| self.searcher.find_raw(s, e)) }
    }

    #[inline]
    fn count(self) -> usize {
        self.it.count(|s, e| {
            // SAFETY: We rely on our generic iterator to return valid start
            // and end pointers.
            unsafe { self.searcher.count_raw(s, e) }
        })
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.it.size_hint()
    }
}

impl<'a, 'h> DoubleEndedIterator for OneIter<'a, 'h> {
    #[inline]
    fn next_back(&mut self) -> Option<usize> {
        // SAFETY: We rely on the generic iterator to provide valid start
        // and end pointers, but we guarantee that any pointer returned by
        // 'rfind_raw' falls within the bounds of the start and end pointer.
        unsafe { self.it.next_back(|s, e| self.searcher.rfind_raw(s, e)) }
    }
}

impl<'a, 'h> core::iter::FusedIterator for OneIter<'a, 'h> {}

/// Finds all occurrences of two bytes in a haystack.
///
/// That is, this reports matches of one of two possible bytes. For example,
/// searching for `a` or `b` in `afoobar` would report matches at offsets `0`,
/// `4` and `5`.
#[derive(Clone, Copy, Debug)]
pub struct Two(generic::Two<uint8x16_t>);

impl Two {
    /// Create a new searcher that finds occurrences of the needle bytes given.
    ///
    /// This particular searcher is specialized to use neon vector instructions
    /// that typically make it quite fast.
    ///
    /// If neon is unavailable in the current environment, then `None` is
    /// returned.
    #[inline]
    pub fn new(needle1: u8, needle2: u8) -> Option<Two> {
        if Two::is_available() {
            // SAFETY: we check that neon is available above.
            unsafe { Some(Two::new_unchecked(needle1, needle2)) }
        } else {
            None
        }
    }

    /// Create a new finder specific to neon vectors and routines without
    /// checking that neon is available.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that it is safe to execute `neon` instructions
    /// in the current environment.
    ///
    /// Note that it is a common misconception that if one compiles for an
    /// `x86_64` target, then they therefore automatically have access to neon
    /// instructions. While this is almost always the case, it isn't true in
    /// 100% of cases.
    #[target_feature(enable = "neon")]
    #[inline]
    pub unsafe fn new_unchecked(needle1: u8, needle2: u8) -> Two {
        Two(generic::Two::new(needle1, needle2))
    }

    /// Returns true when this implementation is available in the current
    /// environment.
    ///
    /// When this is true, it is guaranteed that [`Two::new`] will return
    /// a `Some` value. Similarly, when it is false, it is guaranteed that
    /// `Two::new` will return a `None` value.
    ///
    /// Note also that for the lifetime of a single program, if this returns
    /// true then it will always return true.
    #[inline]
    pub fn is_available() -> bool {
        #[cfg(target_feature = "neon")]
        {
            true
        }
        #[cfg(not(target_feature = "neon"))]
        {
            false
        }
    }

    /// Return the first occurrence of one of the needle bytes in the given
    /// haystack. If no such occurrence exists, then `None` is returned.
    ///
    /// The occurrence is reported as an offset into `haystack`. Its maximum
    /// value is `haystack.len() - 1`.
    #[inline]
    pub fn find(&self, haystack: &[u8]) -> Option<usize> {
        // SAFETY: `find_raw` guarantees that if a pointer is returned, it
        // falls within the bounds of the start and end pointers.
        unsafe {
            generic::search_slice_with_raw(haystack, |s, e| {
                self.find_raw(s, e)
            })
        }
    }

    /// Return the last occurrence of one of the needle bytes in the given
    /// haystack. If no such occurrence exists, then `None` is returned.
    ///
    /// The occurrence is reported as an offset into `haystack`. Its maximum
    /// value is `haystack.len() - 1`.
    #[inline]
    pub fn rfind(&self, haystack: &[u8]) -> Option<usize> {
        // SAFETY: `rfind_raw` guarantees that if a pointer is returned, it
        // falls within the bounds of the start and end pointers.
        unsafe {
            generic::search_slice_with_raw(haystack, |s, e| {
                self.rfind_raw(s, e)
            })
        }
    }

    /// Like `find`, but accepts and returns raw pointers.
    ///
    /// When a match is found, the pointer returned is guaranteed to be
    /// `>= start` and `< end`.
    ///
    /// This routine is useful if you're already using raw pointers and would
    /// like to avoid converting back to a slice before executing a search.
    ///
    /// # Safety
    ///
    /// * Both `start` and `end` must be valid for reads.
    /// * Both `start` and `end` must point to an initialized value.
    /// * Both `start` and `end` must point to the same allocated object and
    /// must either be in bounds or at most one byte past the end of the
    /// allocated object.
    /// * Both `start` and `end` must be _derived from_ a pointer to the same
    /// object.
    /// * The distance between `start` and `end` must not overflow `isize`.
    /// * The distance being in bounds must not rely on "wrapping around" the
    /// address space.
    ///
    /// Note that callers may pass a pair of pointers such that `start >= end`.
    /// In that case, `None` will always be returned.
    #[inline]
    pub unsafe fn find_raw(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        if start >= end {
            return None;
        }
        if end.distance(start) < uint8x16_t::BYTES {
            // SAFETY: We require the caller to pass valid start/end pointers.
            return generic::fwd_byte_by_byte(start, end, |b| {
                b == self.0.needle1() || b == self.0.needle2()
            });
        }
        // SAFETY: Building a `Two` means it's safe to call 'neon' routines.
        // Also, we've checked that our haystack is big enough to run on the
        // vector routine. Pointer validity is caller's responsibility.
        self.find_raw_impl(start, end)
    }

    /// Like `rfind`, but accepts and returns raw pointers.
    ///
    /// When a match is found, the pointer returned is guaranteed to be
    /// `>= start` and `< end`.
    ///
    /// This routine is useful if you're already using raw pointers and would
    /// like to avoid converting back to a slice before executing a search.
    ///
    /// # Safety
    ///
    /// * Both `start` and `end` must be valid for reads.
    /// * Both `start` and `end` must point to an initialized value.
    /// * Both `start` and `end` must point to the same allocated object and
    /// must either be in bounds or at most one byte past the end of the
    /// allocated object.
    /// * Both `start` and `end` must be _derived from_ a pointer to the same
    /// object.
    /// * The distance between `start` and `end` must not overflow `isize`.
    /// * The distance being in bounds must not rely on "wrapping around" the
    /// address space.
    ///
    /// Note that callers may pass a pair of pointers such that `start >= end`.
    /// In that case, `None` will always be returned.
    #[inline]
    pub unsafe fn rfind_raw(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        if start >= end {
            return None;
        }
        if end.distance(start) < uint8x16_t::BYTES {
            // SAFETY: We require the caller to pass valid start/end pointers.
            return generic::rev_byte_by_byte(start, end, |b| {
                b == self.0.needle1() || b == self.0.needle2()
            });
        }
        // SAFETY: Building a `Two` means it's safe to call 'neon' routines.
        // Also, we've checked that our haystack is big enough to run on the
        // vector routine. Pointer validity is caller's responsibility.
        self.rfind_raw_impl(start, end)
    }

    /// Execute a search using neon vectors and routines.
    ///
    /// # Safety
    ///
    /// Same as [`Two::find_raw`], except the distance between `start` and
    /// `end` must be at least the size of a neon vector (in bytes).
    ///
    /// (The target feature safety obligation is automatically fulfilled by
    /// virtue of being a method on `Two`, which can only be constructed
    /// when it is safe to call `neon` routines.)
    #[target_feature(enable = "neon")]
    #[inline]
    unsafe fn find_raw_impl(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        self.0.find_raw(start, end)
    }

    /// Execute a search using neon vectors and routines.
    ///
    /// # Safety
    ///
    /// Same as [`Two::rfind_raw`], except the distance between `start` and
    /// `end` must be at least the size of a neon vector (in bytes).
    ///
    /// (The target feature safety obligation is automatically fulfilled by
    /// virtue of being a method on `Two`, which can only be constructed
    /// when it is safe to call `neon` routines.)
    #[target_feature(enable = "neon")]
    #[inline]
    unsafe fn rfind_raw_impl(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        self.0.rfind_raw(start, end)
    }

    /// Returns an iterator over all occurrences of the needle bytes in the
    /// given haystack.
    ///
    /// The iterator returned implements `DoubleEndedIterator`. This means it
    /// can also be used to find occurrences in reverse order.
    #[inline]
    pub fn iter<'a, 'h>(&'a self, haystack: &'h [u8]) -> TwoIter<'a, 'h> {
        TwoIter { searcher: self, it: generic::Iter::new(haystack) }
    }
}

/// An iterator over all occurrences of two possible bytes in a haystack.
///
/// This iterator implements `DoubleEndedIterator`, which means it can also be
/// used to find occurrences in reverse order.
///
/// This iterator is created by the [`Two::iter`] method.
///
/// The lifetime parameters are as follows:
///
/// * `'a` refers to the lifetime of the underlying [`Two`] searcher.
/// * `'h` refers to the lifetime of the haystack being searched.
#[derive(Clone, Debug)]
pub struct TwoIter<'a, 'h> {
    searcher: &'a Two,
    it: generic::Iter<'h>,
}

impl<'a, 'h> Iterator for TwoIter<'a, 'h> {
    type Item = usize;

    #[inline]
    fn next(&mut self) -> Option<usize> {
        // SAFETY: We rely on the generic iterator to provide valid start
        // and end pointers, but we guarantee that any pointer returned by
        // 'find_raw' falls within the bounds of the start and end pointer.
        unsafe { self.it.next(|s, e| self.searcher.find_raw(s, e)) }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.it.size_hint()
    }
}

impl<'a, 'h> DoubleEndedIterator for TwoIter<'a, 'h> {
    #[inline]
    fn next_back(&mut self) -> Option<usize> {
        // SAFETY: We rely on the generic iterator to provide valid start
        // and end pointers, but we guarantee that any pointer returned by
        // 'rfind_raw' falls within the bounds of the start and end pointer.
        unsafe { self.it.next_back(|s, e| self.searcher.rfind_raw(s, e)) }
    }
}

impl<'a, 'h> core::iter::FusedIterator for TwoIter<'a, 'h> {}

/// Finds all occurrences of three bytes in a haystack.
///
/// That is, this reports matches of one of three possible bytes. For example,
/// searching for `a`, `b` or `o` in `afoobar` would report matches at offsets
/// `0`, `2`, `3`, `4` and `5`.
#[derive(Clone, Copy, Debug)]
pub struct Three(generic::Three<uint8x16_t>);

impl Three {
    /// Create a new searcher that finds occurrences of the needle bytes given.
    ///
    /// This particular searcher is specialized to use neon vector instructions
    /// that typically make it quite fast.
    ///
    /// If neon is unavailable in the current environment, then `None` is
    /// returned.
    #[inline]
    pub fn new(needle1: u8, needle2: u8, needle3: u8) -> Option<Three> {
        if Three::is_available() {
            // SAFETY: we check that neon is available above.
            unsafe { Some(Three::new_unchecked(needle1, needle2, needle3)) }
        } else {
            None
        }
    }

    /// Create a new finder specific to neon vectors and routines without
    /// checking that neon is available.
    ///
    /// # Safety
    ///
    /// Callers must guarantee that it is safe to execute `neon` instructions
    /// in the current environment.
    ///
    /// Note that it is a common misconception that if one compiles for an
    /// `x86_64` target, then they therefore automatically have access to neon
    /// instructions. While this is almost always the case, it isn't true in
    /// 100% of cases.
    #[target_feature(enable = "neon")]
    #[inline]
    pub unsafe fn new_unchecked(
        needle1: u8,
        needle2: u8,
        needle3: u8,
    ) -> Three {
        Three(generic::Three::new(needle1, needle2, needle3))
    }

    /// Returns true when this implementation is available in the current
    /// environment.
    ///
    /// When this is true, it is guaranteed that [`Three::new`] will return
    /// a `Some` value. Similarly, when it is false, it is guaranteed that
    /// `Three::new` will return a `None` value.
    ///
    /// Note also that for the lifetime of a single program, if this returns
    /// true then it will always return true.
    #[inline]
    pub fn is_available() -> bool {
        #[cfg(target_feature = "neon")]
        {
            true
        }
        #[cfg(not(target_feature = "neon"))]
        {
            false
        }
    }

    /// Return the first occurrence of one of the needle bytes in the given
    /// haystack. If no such occurrence exists, then `None` is returned.
    ///
    /// The occurrence is reported as an offset into `haystack`. Its maximum
    /// value is `haystack.len() - 1`.
    #[inline]
    pub fn find(&self, haystack: &[u8]) -> Option<usize> {
        // SAFETY: `find_raw` guarantees that if a pointer is returned, it
        // falls within the bounds of the start and end pointers.
        unsafe {
            generic::search_slice_with_raw(haystack, |s, e| {
                self.find_raw(s, e)
            })
        }
    }

    /// Return the last occurrence of one of the needle bytes in the given
    /// haystack. If no such occurrence exists, then `None` is returned.
    ///
    /// The occurrence is reported as an offset into `haystack`. Its maximum
    /// value is `haystack.len() - 1`.
    #[inline]
    pub fn rfind(&self, haystack: &[u8]) -> Option<usize> {
        // SAFETY: `rfind_raw` guarantees that if a pointer is returned, it
        // falls within the bounds of the start and end pointers.
        unsafe {
            generic::search_slice_with_raw(haystack, |s, e| {
                self.rfind_raw(s, e)
            })
        }
    }

    /// Like `find`, but accepts and returns raw pointers.
    ///
    /// When a match is found, the pointer returned is guaranteed to be
    /// `>= start` and `< end`.
    ///
    /// This routine is useful if you're already using raw pointers and would
    /// like to avoid converting back to a slice before executing a search.
    ///
    /// # Safety
    ///
    /// * Both `start` and `end` must be valid for reads.
    /// * Both `start` and `end` must point to an initialized value.
    /// * Both `start` and `end` must point to the same allocated object and
    /// must either be in bounds or at most one byte past the end of the
    /// allocated object.
    /// * Both `start` and `end` must be _derived from_ a pointer to the same
    /// object.
    /// * The distance between `start` and `end` must not overflow `isize`.
    /// * The distance being in bounds must not rely on "wrapping around" the
    /// address space.
    ///
    /// Note that callers may pass a pair of pointers such that `start >= end`.
    /// In that case, `None` will always be returned.
    #[inline]
    pub unsafe fn find_raw(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        if start >= end {
            return None;
        }
        if end.distance(start) < uint8x16_t::BYTES {
            // SAFETY: We require the caller to pass valid start/end pointers.
            return generic::fwd_byte_by_byte(start, end, |b| {
                b == self.0.needle1()
                    || b == self.0.needle2()
                    || b == self.0.needle3()
            });
        }
        // SAFETY: Building a `Three` means it's safe to call 'neon' routines.
        // Also, we've checked that our haystack is big enough to run on the
        // vector routine. Pointer validity is caller's responsibility.
        self.find_raw_impl(start, end)
    }

    /// Like `rfind`, but accepts and returns raw pointers.
    ///
    /// When a match is found, the pointer returned is guaranteed to be
    /// `>= start` and `< end`.
    ///
    /// This routine is useful if you're already using raw pointers and would
    /// like to avoid converting back to a slice before executing a search.
    ///
    /// # Safety
    ///
    /// * Both `start` and `end` must be valid for reads.
    /// * Both `start` and `end` must point to an initialized value.
    /// * Both `start` and `end` must point to the same allocated object and
    /// must either be in bounds or at most one byte past the end of the
    /// allocated object.
    /// * Both `start` and `end` must be _derived from_ a pointer to the same
    /// object.
    /// * The distance between `start` and `end` must not overflow `isize`.
    /// * The distance being in bounds must not rely on "wrapping around" the
    /// address space.
    ///
    /// Note that callers may pass a pair of pointers such that `start >= end`.
    /// In that case, `None` will always be returned.
    #[inline]
    pub unsafe fn rfind_raw(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        if start >= end {
            return None;
        }
        if end.distance(start) < uint8x16_t::BYTES {
            // SAFETY: We require the caller to pass valid start/end pointers.
            return generic::rev_byte_by_byte(start, end, |b| {
                b == self.0.needle1()
                    || b == self.0.needle2()
                    || b == self.0.needle3()
            });
        }
        // SAFETY: Building a `Three` means it's safe to call 'neon' routines.
        // Also, we've checked that our haystack is big enough to run on the
        // vector routine. Pointer validity is caller's responsibility.
        self.rfind_raw_impl(start, end)
    }

    /// Execute a search using neon vectors and routines.
    ///
    /// # Safety
    ///
    /// Same as [`Three::find_raw`], except the distance between `start` and
    /// `end` must be at least the size of a neon vector (in bytes).
    ///
    /// (The target feature safety obligation is automatically fulfilled by
    /// virtue of being a method on `Three`, which can only be constructed
    /// when it is safe to call `neon` routines.)
    #[target_feature(enable = "neon")]
    #[inline]
    unsafe fn find_raw_impl(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        self.0.find_raw(start, end)
    }

    /// Execute a search using neon vectors and routines.
    ///
    /// # Safety
    ///
    /// Same as [`Three::rfind_raw`], except the distance between `start` and
    /// `end` must be at least the size of a neon vector (in bytes).
    ///
    /// (The target feature safety obligation is automatically fulfilled by
    /// virtue of being a method on `Three`, which can only be constructed
    /// when it is safe to call `neon` routines.)
    #[target_feature(enable = "neon")]
    #[inline]
    unsafe fn rfind_raw_impl(
        &self,
        start: *const u8,
        end: *const u8,
    ) -> Option<*const u8> {
        self.0.rfind_raw(start, end)
    }

    /// Returns an iterator over all occurrences of the needle byte in the
    /// given haystack.
    ///
    /// The iterator returned implements `DoubleEndedIterator`. This means it
    /// can also be used to find occurrences in reverse order.
    #[inline]
    pub fn iter<'a, 'h>(&'a self, haystack: &'h [u8]) -> ThreeIter<'a, 'h> {
        ThreeIter { searcher: self, it: generic::Iter::new(haystack) }
    }
}

/// An iterator over all occurrences of three possible bytes in a haystack.
///
/// This iterator implements `DoubleEndedIterator`, which means it can also be
/// used to find occurrences in reverse order.
///
/// This iterator is created by the [`Three::iter`] method.
///
/// The lifetime parameters are as follows:
///
/// * `'a` refers to the lifetime of the underlying [`Three`] searcher.
/// * `'h` refers to the lifetime of the haystack being searched.
#[derive(Clone, Debug)]
pub struct ThreeIter<'a, 'h> {
    searcher: &'a Three,
    it: generic::Iter<'h>,
}

impl<'a, 'h> Iterator for ThreeIter<'a, 'h> {
    type Item = usize;

    #[inline]
    fn next(&mut self) -> Option<usize> {
        // SAFETY: We rely on the generic iterator to provide valid start
        // and end pointers, but we guarantee that any pointer returned by
        // 'find_raw' falls within the bounds of the start and end pointer.
        unsafe { self.it.next(|s, e| self.searcher.find_raw(s, e)) }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.it.size_hint()
    }
}

impl<'a, 'h> DoubleEndedIterator for ThreeIter<'a, 'h> {
    #[inline]
    fn next_back(&mut self) -> Option<usize> {
        // SAFETY: We rely on the generic iterator to provide valid start
        // and end pointers, but we guarantee that any pointer returned by
        // 'rfind_raw' falls within the bounds of the start and end pointer.
        unsafe { self.it.next_back(|s, e| self.searcher.rfind_raw(s, e)) }
    }
}

impl<'a, 'h> core::iter::FusedIterator for ThreeIter<'a, 'h> {}

#[cfg(test)]
mod tests {
    use super::*;

    define_memchr_quickcheck!(super);

    #[test]
    fn forward_one() {
        crate::tests::memchr::Runner::new(1).forward_iter(
            |haystack, needles| {
                Some(One::new(needles[0])?.iter(haystack).collect())
            },
        )
    }

    #[test]
    fn reverse_one() {
        crate::tests::memchr::Runner::new(1).reverse_iter(
            |haystack, needles| {
                Some(One::new(needles[0])?.iter(haystack).rev().collect())
            },
        )
    }

    #[test]
    fn count_one() {
        crate::tests::memchr::Runner::new(1).count_iter(|haystack, needles| {
            Some(One::new(needles[0])?.iter(haystack).count())
        })
    }

    #[test]
    fn forward_two() {
        crate::tests::memchr::Runner::new(2).forward_iter(
            |haystack, needles| {
                let n1 = needles.get(0).copied()?;
                let n2 = needles.get(1).copied()?;
                Some(Two::new(n1, n2)?.iter(haystack).collect())
            },
        )
    }

    #[test]
    fn reverse_two() {
        crate::tests::memchr::Runner::new(2).reverse_iter(
            |haystack, needles| {
                let n1 = needles.get(0).copied()?;
                let n2 = needles.get(1).copied()?;
                Some(Two::new(n1, n2)?.iter(haystack).rev().collect())
            },
        )
    }

    #[test]
    fn forward_three() {
        crate::tests::memchr::Runner::new(3).forward_iter(
            |haystack, needles| {
                let n1 = needles.get(0).copied()?;
                let n2 = needles.get(1).copied()?;
                let n3 = needles.get(2).copied()?;
                Some(Three::new(n1, n2, n3)?.iter(haystack).collect())
            },
        )
    }

    #[test]
    fn reverse_three() {
        crate::tests::memchr::Runner::new(3).reverse_iter(
            |haystack, needles| {
                let n1 = needles.get(0).copied()?;
                let n2 = needles.get(1).copied()?;
                let n3 = needles.get(2).copied()?;
                Some(Three::new(n1, n2, n3)?.iter(haystack).rev().collect())
            },
        )
    }
}
