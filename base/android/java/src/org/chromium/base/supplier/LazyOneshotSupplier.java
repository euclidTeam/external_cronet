// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;

/**
 * Wraps a lazy-loaded nullable object, notifying observers a single time when the dependency
 * becomes available. This intentionally doesn't extend {@link OneshotSupplier} to support the
 * supplied value being null.
 *
 * @param <T> The type of the wrapped object.
 */
public interface LazyOneshotSupplier<T> {
    /**
     * Add a callback that's called when the object owned by this supplier is available. If the
     * object is already available, the callback will be called at the end of the current message
     * loop.
     *
     * @param callback The callback to be called.
     */
    void onAvailable(Callback<T> callback);

    /**
     * Returns the value currently held or <code>null</code> when none is held. Use {@link
     * #hasValue} to tell if the value is intentionally null.
     */
    @Nullable
    T get();

    /** Returns whether the supplier holds a value currently. */
    boolean hasValue();
}
