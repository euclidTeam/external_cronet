// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.PowerMonitor;
import org.chromium.base.library_loader.LibraryLoader;

/**
 * A helper for running native unit tests (i.e., not browser tests)
 */
public class NativeUnitTest extends NativeTest {
    private static final String TAG = "NativeTest";

    private static final String LIBRARY_UNDER_TEST_NAME =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.LibraryUnderTest";
    private static class NativeUnitTestLibraryLoader extends LibraryLoader {
        static void setLibrariesLoaded() {
            LibraryLoader.setLibrariesLoadedForNativeTests();
        }
    }

    @Override
    public void preCreate(Activity activity) {
        super.preCreate(activity);
        // Necessary because NativeUnitTestActivity uses BaseChromiumApplication which does not
        // initialize ContextUtils.
        ContextUtils.initApplicationContext(activity.getApplicationContext());

        // Necessary because BaseChromiumApplication no longer automatically initializes application
        // tracking.
        ApplicationStatus.initialize(activity.getApplication());

        // Needed by path_utils_unittest.cc
        PathUtils.setPrivateDataDirectorySuffix("chrome");

        // Needed by system_monitor_unittest.cc
        PowerMonitor.createForTests();

        // For NativeActivity based tests,
        // dependency libraries must be loaded before NativeActivity::OnCreate,
        // otherwise loading android.app.lib_name will fail
        String libraryToLoad = activity.getIntent().getStringExtra(LIBRARY_UNDER_TEST_NAME);
        if (libraryToLoad != null) {
            loadLibrary(libraryToLoad);
        } else {
            Log.e(TAG, "No Library provided for native tests! Exiting");
            activity.finish();
        }
    }

    private void loadLibrary(String library) {

        LibraryLoader.setEnvForNative();
        Log.i(TAG, "loading: %s", library);
        System.loadLibrary(library);
        Log.i(TAG, "loaded: %s", library);
        NativeUnitTestLibraryLoader.setLibrariesLoaded();
    }
}
