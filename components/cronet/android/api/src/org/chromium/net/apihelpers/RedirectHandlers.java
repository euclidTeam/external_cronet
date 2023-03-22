<<<<<<< HEAD   (7f0b85 Merge branch 'upstream-import' into upstream-staging)
=======
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

/**
 * Utility class for standard {@link RedirectHandler} implementations. *
 */
public class RedirectHandlers {
    /**
     * Returns a redirect handler that never follows redirects.
     */
    public static RedirectHandler neverFollow() {
        return (info, newLocationUrl) -> false;
    }

    /**
     * Returns a redirect handler that always follows redirects.
     *
     * <p>Note that the maximum number of redirects to follow is still limited internally to prevent
     * infinite looping.
     */
    public static RedirectHandler alwaysFollow() {
        return (info, newLocationUrl) -> true;
    }

    private RedirectHandlers() {}
}
>>>>>>> BRANCH (3e3c7e Import Cronet version 110.0.5481.154)
