// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;

/**
 * A Java wrapper to supply a net::MockCertVerifier which can be then passed
 * into {@link CronetEngine.Builder#setMockCertVerifierForTesting}.
 * The native pointer will be freed when the CronetEngine is torn down.
 */
@JNINamespace("cronet")
public class MockCertVerifier {
    private MockCertVerifier() {}

    /**
     * Creates a new net::MockCertVerifier, and returns a pointer to it.
     * @param certs a String array of certificate filenames in
     *         net::GetTestCertsDirectory() to accept in testing.
     * @return a pointer to the newly created net::MockCertVerifier.
     */
    public static long createMockCertVerifier(String[] certs, boolean knownRoot) {
        return nativeCreateMockCertVerifier(certs, knownRoot,
                TestFilesInstaller.getInstalledPath(ContextUtils.getApplicationContext()));
    }

    /**
     * Creates a new free-for-all net::MockCertVerifier and returns a pointer to it.
     *
     * @return a pointer to the newly created net::MockCertVerifier.
     */
    public static long createFreeForAllMockCertVerifier() {
        return nativeCreateFreeForAllMockCertVerifier();
    }

    private static native long nativeCreateMockCertVerifier(
            String[] certs, boolean knownRoot, String testDataDir);

    private static native long nativeCreateFreeForAllMockCertVerifier();
}
