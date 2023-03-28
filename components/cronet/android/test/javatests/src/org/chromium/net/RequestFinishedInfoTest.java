// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.base.CollectionUtil.newHashSet;
import static org.chromium.net.CronetTestRule.getContext;

import android.net.http.ExperimentalHttpEngine;
import android.net.http.ExperimentalUrlRequest;
import android.net.http.NetworkException;
import android.net.http.RequestFinishedInfo;
import android.net.http.UrlRequest;
import android.net.http.UrlResponseInfo;
import android.os.ConditionVariable;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.MetricsTestUtil.TestExecutor;
import org.chromium.net.MetricsTestUtil.TestRequestFinishedListener;
import org.chromium.net.impl.CronetMetrics;

import java.time.Instant;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

// TODO: Cant cast to experimentalurlrequest for some reason
/**
 * Test RequestFinishedInfo.Listener and the metrics information it provides.
 */
@RunWith(AndroidJUnit4.class)
public class RequestFinishedInfoTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    CronetTestFramework mTestFramework;

    private String mUrl;

    // A subclass of TestRequestFinishedListener to additionally assert that UrlRequest.Callback's
    // terminal callbacks have been invoked at the time of onRequestFinished().
    // See crbug.com/710877.
    private static class AssertCallbackDoneRequestFinishedListener
            extends TestRequestFinishedListener {
        private final TestUrlRequestCallback mCallback;
        public AssertCallbackDoneRequestFinishedListener(TestUrlRequestCallback callback) {
            // Use same executor as request callback to verify stable call order.
            super(callback.getExecutor());
            mCallback = callback;
        }

        @Override
        public void onRequestFinished(RequestFinishedInfo requestInfo) {
            assertTrue(mCallback.isDone());
            super.onRequestFinished(requestInfo);
        }
    };

    @Before
    public void setUp() throws Exception {
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        mUrl = NativeTestServer.getSuccessURL();
        mTestFramework = mTestRule.startCronetTestFramework();
    }

    @After
    public void tearDown() throws Exception {
        mTestFramework.mCronetEngine.shutdown();
        NativeTestServer.shutdownNativeTestServer();
    }

    static class DirectExecutor implements Executor {
        private ConditionVariable mBlock = new ConditionVariable();

        @Override
        public void execute(Runnable task) {
            task.run();
            mBlock.open();
        }

        public void blockUntilDone() {
            mBlock.block();
        }
    }

    static class ThreadExecutor implements Executor {
        private List<Thread> mThreads = new ArrayList<Thread>();

        @Override
        public void execute(Runnable task) {
            Thread newThread = new Thread(task);
            mThreads.add(newThread);
            newThread.start();
        }

        public void joinAll() throws InterruptedException {
            for (Thread thread : mThreads) {
                thread.join();
            }
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListener() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Instant endTime = Instant.now();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, false);
        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(requestInfo.getAnnotations()));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerDirectExecutor() throws Exception {
        DirectExecutor testExecutor = new DirectExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        // Block on the executor, not the listener, since blocking on the listener doesn't work when
        // it's created with a non-default executor.
        testExecutor.blockUntilDone();
        Instant endTime = Instant.now();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, false);
        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(requestInfo.getAnnotations()));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerDifferentThreads() throws Exception {
        TestRequestFinishedListener firstListener = new TestRequestFinishedListener();
        TestRequestFinishedListener secondListener = new TestRequestFinishedListener();
        mTestFramework.mCronetEngine.addRequestFinishedListener(firstListener);
        mTestFramework.mCronetEngine.addRequestFinishedListener(secondListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        firstListener.blockUntilDone();
        secondListener.blockUntilDone();
        Instant endTime = Instant.now();

        RequestFinishedInfo firstRequestInfo = firstListener.getRequestInfo();
        RequestFinishedInfo secondRequestInfo = secondListener.getRequestInfo();

        MetricsTestUtil.checkRequestFinishedInfo(firstRequestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, firstRequestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(
                firstRequestInfo.getMetrics(), startTime, endTime, false);

        MetricsTestUtil.checkRequestFinishedInfo(secondRequestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, secondRequestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(
                secondRequestInfo.getMetrics(), startTime, endTime, false);

        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(firstRequestInfo.getAnnotations()));
        assertEquals(newHashSet("request annotation", this),
                new HashSet<Object>(secondRequestInfo.getAnnotations()));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerFailedRequest() throws Exception {
        String connectionRefusedUrl = "http://127.0.0.1:3";
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                connectionRefusedUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertTrue(callback.mOnErrorCalled);
        requestFinishedListener.blockUntilDone();
        Instant endTime = Instant.now();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertNotNull("RequestFinishedInfo.Listener must be called", requestInfo);
        assertEquals(connectionRefusedUrl, requestInfo.getUrl());
        assertTrue(requestInfo.getAnnotations().isEmpty());
        assertEquals(RequestFinishedInfo.FAILED, requestInfo.getFinishedReason());
        assertNotNull(requestInfo.getException());
        assertEquals(NetworkException.ERROR_CONNECTION_REFUSED,
                ((NetworkException) requestInfo.getException()).getErrorCode());
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", metrics);

        // Check the timing metrics
        assertNotNull(metrics.getRequestStart());
        MetricsTestUtil.assertAfter(metrics.getRequestStart(), startTime);
        MetricsTestUtil.checkNoConnectTiming(metrics);
        assertNull(metrics.getSendingStart());
        assertNull(metrics.getSendingEnd());
        assertNull(metrics.getResponseStart());
        assertNotNull(metrics.getRequestEnd());
        MetricsTestUtil.assertAfter(endTime, metrics.getRequestEnd());
        MetricsTestUtil.assertAfter(metrics.getRequestEnd(), metrics.getRequestStart());
        assertTrue(metrics.getSentByteCount() == 0);
        assertTrue(metrics.getReceivedByteCount() == 0);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerRemoved() throws Exception {
        TestExecutor testExecutor = new TestExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        UrlRequest request = urlRequestBuilder.build();
        mTestFramework.mCronetEngine.removeRequestFinishedListener(requestFinishedListener);
        request.start();
        callback.blockForDone();
        testExecutor.runAllTasks();

        assertNull("RequestFinishedInfo.Listener must not be called",
                requestFinishedListener.getRequestInfo());
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testRequestFinishedListenerCanceledRequest() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback() {
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                super.onResponseStarted(request, info);
                request.cancel();
            }
        };
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Instant endTime = Instant.now();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.CANCELED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, false);

        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(requestInfo.getAnnotations()));
    }

    private static class RejectAllTasksExecutor implements Executor {
        @Override
        public void execute(Runnable task) {
            throw new RejectedExecutionException();
        }
    }

    // Checks that CronetURLRequestAdapter::DestroyOnNetworkThread() doesn't crash when metrics
    // collection is enabled and the URLRequest hasn't been created. See http://crbug.com/675629.
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testExceptionInRequestStart() throws Exception {
        // The listener in this test shouldn't get any tasks.
        Executor executor = new RejectAllTasksExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(executor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        // Empty headers are invalid and will cause start() to throw an exception.
        UrlRequest request = urlRequestBuilder.addHeader("", "").build();
        try {
            request.start();
            fail("UrlRequest.start() should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            assertEquals("Invalid header =", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testMetricsGetters() throws Exception {
        long requestStart = 1;
        long dnsStart = 2;
        long dnsEnd = -1;
        long connectStart = 4;
        long connectEnd = 5;
        long sslStart = 6;
        long sslEnd = 7;
        long sendingStart = 8;
        long sendingEnd = 9;
        long pushStart = 10;
        long pushEnd = 11;
        long responseStart = 12;
        long requestEnd = 13;
        boolean socketReused = true;
        long sentByteCount = 14;
        long receivedByteCount = 15;
        // Make sure nothing gets reordered inside the Metrics class
        RequestFinishedInfo.Metrics metrics = new CronetMetrics(requestStart, dnsStart, dnsEnd,
                connectStart, connectEnd, sslStart, sslEnd, sendingStart, sendingEnd, pushStart,
                pushEnd, responseStart, requestEnd, socketReused, sentByteCount, receivedByteCount);
        assertEquals(Instant.ofEpochMilli(requestStart), metrics.getRequestStart());
        // -1 timestamp should translate to null
        assertNull(metrics.getDnsEnd());
        assertEquals(Instant.ofEpochMilli(dnsStart), metrics.getDnsStart());
        assertEquals(Instant.ofEpochMilli(connectStart), metrics.getConnectStart());
        assertEquals(Instant.ofEpochMilli(connectEnd), metrics.getConnectEnd());
        assertEquals(Instant.ofEpochMilli(sslStart), metrics.getSslStart());
        assertEquals(Instant.ofEpochMilli(sslEnd), metrics.getSslEnd());
        assertEquals(Instant.ofEpochMilli(pushStart), metrics.getPushStart());
        assertEquals(Instant.ofEpochMilli(pushEnd), metrics.getPushEnd());
        assertEquals(Instant.ofEpochMilli(responseStart), metrics.getResponseStart());
        assertEquals(Instant.ofEpochMilli(requestEnd), metrics.getRequestEnd());
        assertEquals(socketReused, metrics.getSocketReused());
        assertEquals(sentByteCount, (long) metrics.getSentByteCount());
        assertEquals(receivedByteCount, (long) metrics.getReceivedByteCount());
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testOrderSuccessfulRequest() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Instant endTime = Instant.now();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, false);
        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(requestInfo.getAnnotations()));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @RequiresMinApi(11)
    public void testUpdateAnnotationOnSucceeded() throws Exception {
        // The annotation that is updated in onSucceeded() callback.
        AtomicBoolean requestAnnotation = new AtomicBoolean(false);
        final TestUrlRequestCallback callback = new TestUrlRequestCallback() {
            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                // Add processing information to request annotation.
                requestAnnotation.set(true);
                super.onSucceeded(request, info);
            }
        };
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.addRequestAnnotation(requestAnnotation)
                .setRequestFinishedListener(requestFinishedListener)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Instant endTime = Instant.now();
        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, false);
        // Check that annotation got updated in onSucceeded() callback.
        assertEquals(requestAnnotation, requestInfo.getAnnotations().iterator().next());
        assertTrue(requestAnnotation.get());
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    // Tests a failed request where the error originates from Java.
    public void testOrderFailedRequestJava() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback() {
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                throw new RuntimeException("make this request fail");
            }
        };
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        UrlRequest.Builder urlRequestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertTrue(callback.mOnErrorCalled);
        requestFinishedListener.blockUntilDone();
        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertNotNull("RequestFinishedInfo.Listener must be called", requestInfo);
        assertEquals(mUrl, requestInfo.getUrl());
        assertTrue(requestInfo.getAnnotations().isEmpty());
        assertEquals(RequestFinishedInfo.FAILED, requestInfo.getFinishedReason());
        assertNotNull(requestInfo.getException());
        assertEquals("Exception received from UrlRequest.Callback",
                requestInfo.getException().getMessage());
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", metrics);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    // Tests a failed request where the error originates from native code.
    public void testOrderFailedRequestNative() throws Exception {
        String connectionRefusedUrl = "http://127.0.0.1:3";
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        UrlRequest.Builder urlRequestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                connectionRefusedUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertTrue(callback.mOnErrorCalled);
        requestFinishedListener.blockUntilDone();
        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertNotNull("RequestFinishedInfo.Listener must be called", requestInfo);
        assertEquals(connectionRefusedUrl, requestInfo.getUrl());
        assertTrue(requestInfo.getAnnotations().isEmpty());
        assertEquals(RequestFinishedInfo.FAILED, requestInfo.getFinishedReason());
        assertNotNull(requestInfo.getException());
        assertEquals(NetworkException.ERROR_CONNECTION_REFUSED,
                ((NetworkException) requestInfo.getException()).getErrorCode());
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", metrics);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testOrderCanceledRequest() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback() {
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                super.onResponseStarted(request, info);
                request.cancel();
            }
        };

        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        mUrl, callback, callback.getExecutor());
        Instant startTime = Instant.now();
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Instant endTime = Instant.now();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, mUrl, startTime, endTime);
        assertEquals(RequestFinishedInfo.CANCELED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, false);

        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(requestInfo.getAnnotations()));
    }
}
