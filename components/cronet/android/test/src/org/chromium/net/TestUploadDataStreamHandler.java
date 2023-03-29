// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.net.http.HttpEngine;
import android.os.ConditionVariable;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.net.impl.CronetUrlRequestContext;

/**
 * A wrapper class on top of the native net::UploadDataStream. This class is
 * used in tests to drive the native UploadDataStream directly.
 */
@JNINamespace("cronet")
public final class TestUploadDataStreamHandler {
    private final CronetTestUtil.NetworkThreadTestConnector mNetworkThreadTestConnector;
    private final HttpEngine mCronetEngine;
    private long mTestUploadDataStreamHandler;
    private ConditionVariable mWaitInitCalled = new ConditionVariable();
    private ConditionVariable mWaitInitComplete = new ConditionVariable();
    private ConditionVariable mWaitReadComplete = new ConditionVariable();
    private ConditionVariable mWaitResetComplete = new ConditionVariable();
    // Waits for checkIfInitCallbackInvoked() returns result asynchronously.
    private ConditionVariable mWaitCheckInit = new ConditionVariable();
    // Waits for checkIfReadCallbackInvoked() returns result asynchronously.
    private ConditionVariable mWaitCheckRead = new ConditionVariable();
    // If true, init completes synchronously.
    private boolean mInitCompletedSynchronously;
    private String mData = "";

    public TestUploadDataStreamHandler(Context context, final long uploadDataStream) {
        mCronetEngine = new HttpEngine.Builder(context).build();
        mNetworkThreadTestConnector = new CronetTestUtil.NetworkThreadTestConnector(mCronetEngine);
        CronetUrlRequestContext requestContext = (CronetUrlRequestContext) mCronetEngine;
        mTestUploadDataStreamHandler = nativeCreateTestUploadDataStreamHandler(
                uploadDataStream, requestContext.getUrlRequestContextAdapter());
    }

    public void destroyNativeObjects() {
        if (mTestUploadDataStreamHandler != 0) {
            nativeDestroy(mTestUploadDataStreamHandler);
            mTestUploadDataStreamHandler = 0;
            mNetworkThreadTestConnector.shutdown();
            mCronetEngine.shutdown();
        }
    }

    /**
     * Init and returns whether init completes synchronously.
     */
    public boolean init() {
        mData = "";
        nativeInit(mTestUploadDataStreamHandler);
        mWaitInitCalled.block();
        mWaitInitCalled.close();
        return mInitCompletedSynchronously;
    }

    public void read() {
        nativeRead(mTestUploadDataStreamHandler);
    }

    public void reset() {
        mData = "";
        nativeReset(mTestUploadDataStreamHandler);
        mWaitResetComplete.block();
        mWaitResetComplete.close();
    }

    /**
     * Checks that {@link #onInitCompleted} has not invoked asynchronously
     * by the native UploadDataStream.
     */
    public void checkInitCallbackNotInvoked() {
        nativeCheckInitCallbackNotInvoked(mTestUploadDataStreamHandler);
        mWaitCheckInit.block();
        mWaitCheckInit.close();
    }

    /**
     * Checks that {@link #onReadCompleted} has not been invoked asynchronously
     * by the native UploadDataStream.
     */
    public void checkReadCallbackNotInvoked() {
        nativeCheckReadCallbackNotInvoked(mTestUploadDataStreamHandler);
        mWaitCheckRead.block();
        mWaitCheckRead.close();
    }

    public String getData() {
        return mData;
    }

    public void waitForReadComplete() {
        mWaitReadComplete.block();
        mWaitReadComplete.close();
    }

    public void waitForInitComplete() {
        mWaitInitComplete.block();
        mWaitInitComplete.close();
    }

    // Called on network thread.
    @CalledByNative
    private void onInitCalled(int res) {
        if (res == 0) {
            mInitCompletedSynchronously = true;
        } else {
            mInitCompletedSynchronously = false;
        }
        mWaitInitCalled.open();
    }

    // Called on network thread.
    @CalledByNative
    private void onReadCompleted(int bytesRead, String data) {
        mData = data;
        mWaitReadComplete.open();
    }

    // Called on network thread.
    @CalledByNative
    private void onInitCompleted(int res) {
        // If init() completed synchronously, waitForInitComplete() will
        // not be invoked in the test, so skip mWaitInitComplete.open().
        if (!mInitCompletedSynchronously) {
            mWaitInitComplete.open();
        }
    }

    // Called on network thread.
    @CalledByNative
    private void onResetCompleted() {
        mWaitResetComplete.open();
    }

    // Called on network thread.
    @CalledByNative
    private void onCheckInitCallbackNotInvoked(boolean initCallbackNotInvoked) {
        Assert.assertTrue(initCallbackNotInvoked);
        mWaitCheckInit.open();
    }

    // Called on network thread.
    @CalledByNative
    private void onCheckReadCallbackNotInvoked(boolean readCallbackNotInvoked) {
        Assert.assertTrue(readCallbackNotInvoked);
        mWaitCheckRead.open();
    }

    @NativeClassQualifiedName("TestUploadDataStreamHandler")
    private native void nativeInit(long nativePtr);

    @NativeClassQualifiedName("TestUploadDataStreamHandler")
    private native void nativeRead(long nativePtr);

    @NativeClassQualifiedName("TestUploadDataStreamHandler")
    private native void nativeReset(long nativePtr);

    @NativeClassQualifiedName("TestUploadDataStreamHandler")
    private native void nativeCheckInitCallbackNotInvoked(
            long nativePtr);

    @NativeClassQualifiedName("TestUploadDataStreamHandler")
    private native void nativeCheckReadCallbackNotInvoked(
            long nativePtr);

    @NativeClassQualifiedName("TestUploadDataStreamHandler")
    private native void nativeDestroy(long nativePtr);

    private native long nativeCreateTestUploadDataStreamHandler(
            long uploadDataStream, long contextAdapter);
}
