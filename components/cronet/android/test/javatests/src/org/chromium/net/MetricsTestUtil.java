/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.net.http.RequestFinishedInfo;
import android.os.ConditionVariable;

import java.time.Instant;
import java.util.LinkedList;
import java.util.NoSuchElementException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Classes which are useful for testing Cronet's metrics implementation and are needed in more than
 * one test file.
 */
public class MetricsTestUtil {
    /**
     * Executor which runs tasks only when told to with runAllTasks().
     */
    public static class TestExecutor implements Executor {
        private final LinkedList<Runnable> mTaskQueue = new LinkedList<Runnable>();

        @Override
        public void execute(Runnable task) {
            mTaskQueue.add(task);
        }

        public void runAllTasks() {
            try {
                while (mTaskQueue.size() > 0) {
                    mTaskQueue.remove().run();
                }
            } catch (NoSuchElementException e) {
                throw new RuntimeException("Task was removed during iteration", e);
            }
        }
    }

    /**
     * RequestFinishedInfo.Listener for testing, which saves the RequestFinishedInfo
     */
    public static class TestRequestFinishedListener extends RequestFinishedInfo.Listener {
        private final ConditionVariable mBlock;
        private RequestFinishedInfo mRequestInfo;

        // TODO(mgersh): it's weird that you can use either this constructor or blockUntilDone() but
        // not both. Either clean it up or document why it has to work this way.
        public TestRequestFinishedListener(Executor executor) {
            super(executor);
            mBlock = new ConditionVariable();
        }

        public TestRequestFinishedListener() {
            super(Executors.newSingleThreadExecutor());
            mBlock = new ConditionVariable();
        }

        public RequestFinishedInfo getRequestInfo() {
            return mRequestInfo;
        }

        @Override
        public void onRequestFinished(RequestFinishedInfo requestInfo) {
            assertNull("onRequestFinished called repeatedly", mRequestInfo);
            assertNotNull(requestInfo);
            mRequestInfo = requestInfo;
            mBlock.open();
        }

        public void blockUntilDone() {
            mBlock.block();
        }

        public void reset() {
            mBlock.close();
            mRequestInfo = null;
        }
    }

    // Helper method to assert date1 is equals to or after date2.
    // Some implementation of java.util.Date broke the symmetric property, so
    // check both directions.
    public static void assertAfter(Instant date1, Instant date2) {
        assertTrue("date1: " + date1 + ", date2: " + date2,
                date1.isAfter(date2) || date1.equals(date2) || date2.equals(date1));
    }

    /**
     * Check existence of all the timing metrics that apply to most test requests,
     * except those that come from net::LoadTimingInfo::ConnectTiming.
     * Also check some timing differences, focusing on things we can't check with asserts in the
     * CronetMetrics constructor.
     * Don't check push times here.
     */
    public static void checkTimingMetrics(
            RequestFinishedInfo.Metrics metrics, Instant startTime, Instant endTime) {
        assertNotNull(metrics.getRequestStart());
        assertAfter(metrics.getRequestStart(), startTime);
        assertNotNull(metrics.getSendingStart());
        assertAfter(metrics.getSendingStart(), startTime);
        assertNotNull(metrics.getSendingEnd());
        assertAfter(endTime, metrics.getSendingEnd());
        assertNotNull(metrics.getResponseStart());
        assertAfter(metrics.getResponseStart(), startTime);
        assertNotNull(metrics.getRequestEnd());
        assertAfter(endTime, metrics.getRequestEnd());
        assertAfter(metrics.getRequestEnd(), metrics.getRequestStart());
    }

    /**
     * Check that the timing metrics which come from net::LoadTimingInfo::ConnectTiming exist,
     * except SSL times in the case of non-https requests.
     */
    public static void checkHasConnectTiming(
            RequestFinishedInfo.Metrics metrics, Instant startTime, Instant endTime, boolean isSsl) {
        assertNotNull(metrics.getDnsStart());
        assertAfter(metrics.getDnsStart(), startTime);
        assertNotNull(metrics.getDnsEnd());
        assertAfter(endTime, metrics.getDnsEnd());
        assertNotNull(metrics.getConnectStart());
        assertAfter(metrics.getConnectStart(), startTime);
        assertNotNull(metrics.getConnectEnd());
        assertAfter(endTime, metrics.getConnectEnd());
        if (isSsl) {
            assertNotNull(metrics.getSslStart());
            assertAfter(metrics.getSslStart(), startTime);
            assertNotNull(metrics.getSslEnd());
            assertAfter(endTime, metrics.getSslEnd());
        } else {
            assertNull(metrics.getSslStart());
            assertNull(metrics.getSslEnd());
        }
    }

    /**
     * Check that the timing metrics from net::LoadTimingInfo::ConnectTiming don't exist.
     */
    public static void checkNoConnectTiming(RequestFinishedInfo.Metrics metrics) {
        assertNull(metrics.getDnsStart());
        assertNull(metrics.getDnsEnd());
        assertNull(metrics.getSslStart());
        assertNull(metrics.getSslEnd());
        assertNull(metrics.getConnectStart());
        assertNull(metrics.getConnectEnd());
    }

    /**
     * Check that RequestFinishedInfo looks the way it should look for a normal successful request.
     */
    public static void checkRequestFinishedInfo(
            RequestFinishedInfo info, String url, Instant startTime, Instant endTime) {
        assertNotNull("RequestFinishedInfo.Listener must be called", info);
        assertEquals(url, info.getUrl());
        assertNotNull(info.getResponseInfo());
        assertNull(info.getException());
        RequestFinishedInfo.Metrics metrics = info.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", metrics);
        // Check old (deprecated) timing metrics
//        assertTrue(metrics.getTotalTimeMs() >= 0);
//        assertTrue(metrics.getTotalTimeMs() >= metrics.getTtfbMs());
        // Check new timing metrics
        checkTimingMetrics(metrics, startTime, endTime);
        assertNull(metrics.getPushStart());
        assertNull(metrics.getPushEnd());
        // Check data use metrics
        assertTrue(metrics.getSentByteCount() > 0);
        assertTrue(metrics.getReceivedByteCount() > 0);
    }
}
