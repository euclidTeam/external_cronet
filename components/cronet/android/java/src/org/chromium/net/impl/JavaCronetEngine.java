// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static android.os.Process.THREAD_PRIORITY_BACKGROUND;
import static android.os.Process.THREAD_PRIORITY_MORE_FAVORABLE;

import android.util.Log;

import org.chromium.net.BidirectionalStream;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.NetworkQualityRttListener;
import org.chromium.net.NetworkQualityThroughputListener;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UrlRequest;
import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.CronetLogger.CronetVersion;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
/**
 * {@link java.net.HttpURLConnection} backed CronetEngine.
 *
 * <p>Does not support netlogs, transferred data measurement, bidistream, cache, or priority.
 */
public final class JavaCronetEngine extends CronetEngineBase {
    private static final String TAG = JavaCronetEngine.class.getSimpleName();

    private final String mUserAgent;
    private final ExecutorService mExecutorService;
    private final int mCronetEngineId;
    private final CronetLogger mLogger;
    private final AtomicInteger mActiveRequestCount = new AtomicInteger();

    public JavaCronetEngine(CronetEngineBuilderImpl builder) {
        mCronetEngineId = hashCode();
        // On android, all background threads (and all threads that are part
        // of background processes) are put in a cgroup that is allowed to
        // consume up to 5% of CPU - these worker threads spend the vast
        // majority of their time waiting on I/O, so making them contend with
        // background applications for a slice of CPU doesn't make much sense.
        // We want to hurry up and get idle.
        final int threadPriority =
                builder.threadPriority(THREAD_PRIORITY_BACKGROUND + THREAD_PRIORITY_MORE_FAVORABLE);
        this.mUserAgent = builder.getUserAgent();
        this.mExecutorService = new ThreadPoolExecutor(10, 20, 50, TimeUnit.SECONDS,
                new LinkedBlockingQueue<Runnable>(), new ThreadFactory() {
                    @Override
                    public Thread newThread(final Runnable r) {
                        return Executors.defaultThreadFactory().newThread(new Runnable() {
                            @Override
                            public void run() {
                                Thread.currentThread().setName("JavaCronetEngine");
                                android.os.Process.setThreadPriority(threadPriority);
                                r.run();
                            }
                        });
                    }
                });
        mLogger = CronetLoggerFactory.createLogger(
                builder.getContext(), CronetSource.CRONET_SOURCE_FALLBACK);
        try {
            mLogger.logCronetEngineCreation(mCronetEngineId, new CronetEngineBuilderInfo(builder),
                    buildCronetVersion(), CronetSource.CRONET_SOURCE_FALLBACK);
        } catch (RuntimeException e) {
            // Handle any issue gracefully, we should never crash due failures while logging.
            Log.e(TAG, "Error while trying to log JavaCronetEngine creation: ", e);
        }
    }

    /**
     * Increment the number of active requests.
     */
    void incrementActiveRequestCount() {
        mActiveRequestCount.incrementAndGet();
    }

    /**
     * Decrement the number of active requests.
     */
    void decrementActiveRequestCount() {
        mActiveRequestCount.decrementAndGet();
    }

    int getCronetEngineId() {
        return mCronetEngineId;
    }

    CronetLogger getCronetLogger() {
        return mLogger;
    }

    @Override
    public UrlRequestBase createRequest(String url, UrlRequest.Callback callback, Executor executor,
            int priority, Collection<Object> connectionAnnotations, boolean disableCache,
            boolean disableConnectionMigration, boolean allowDirectExecutor,
            boolean trafficStatsTagSet, int trafficStatsTag, boolean trafficStatsUidSet,
            int trafficStatsUid, RequestFinishedInfo.Listener requestFinishedListener,
            int idempotency, long networkHandle) {
        if (networkHandle != DEFAULT_NETWORK_HANDLE) {
            throw new UnsupportedOperationException(
                    "The multi-network API is not supported by the Java implementation "
                    + "of Cronet Engine");
        }
        return new JavaUrlRequest(this, callback, mExecutorService, executor, url, mUserAgent,
                allowDirectExecutor, trafficStatsTagSet, trafficStatsTag, trafficStatsUidSet,
                trafficStatsUid);
    }

    @Override
    protected ExperimentalBidirectionalStream createBidirectionalStream(String url,
            BidirectionalStream.Callback callback, Executor executor, String httpMethod,
            List<Map.Entry<String, String>> requestHeaders, @StreamPriority int priority,
            boolean delayRequestHeadersUntilFirstFlush, Collection<Object> connectionAnnotations,
            boolean trafficStatsTagSet, int trafficStatsTag, boolean trafficStatsUidSet,
            int trafficStatsUid, long networkHandle) {
        throw new UnsupportedOperationException(
                "Can't create a bidi stream - httpurlconnection doesn't have those APIs");
    }

    @Override
    public ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, BidirectionalStream.Callback callback, Executor executor) {
        throw new UnsupportedOperationException(
                "The bidirectional stream API is not supported by the Java implementation "
                + "of Cronet Engine");
    }

    @Override
    public String getVersionString() {
        return "CronetHttpURLConnection/" + ImplVersion.getCronetVersionWithLastChange();
    }

    private CronetVersion buildCronetVersion() {
        String version = getVersionString();
        // getVersionString()'s output looks like "Cronet/w.x.y.z@hash". CronetVersion only cares
        // about the "w.x.y.z" bit.
        version = version.split("/")[1];
        version = version.split("@")[0];
        return new CronetVersion(version);
    }

    @Override
    public void shutdown() {
        mExecutorService.shutdown();
    }

    @Override
    public void startNetLogToFile(String fileName, boolean logAll) {}

    @Override
    public void startNetLogToDisk(String dirPath, boolean logAll, int maxSize) {}

    @Override
    public void stopNetLog() {}

    @Override
    public byte[] getGlobalMetricsDeltas() {
        return new byte[0];
    }

    @Override
    public int getEffectiveConnectionType() {
        return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    }

    @Override
    public int getHttpRttMs() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    @Override
    public int getTransportRttMs() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    @Override
    public int getDownstreamThroughputKbps() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    @Override
    public int getActiveRequestCount() {
        return mActiveRequestCount.get();
    }

    @Override
    public void bindToNetwork(long networkHandle) {
        throw new UnsupportedOperationException(
                "The multi-network API is not supported by the Java implementation "
                + "of Cronet Engine");
    }

    @Override
    public void configureNetworkQualityEstimatorForTesting(boolean useLocalHostRequests,
            boolean useSmallerResponses, boolean disableOfflineCheck) {}

    @Override
    public void addRttListener(NetworkQualityRttListener listener) {}

    @Override
    public void removeRttListener(NetworkQualityRttListener listener) {}

    @Override
    public void addThroughputListener(NetworkQualityThroughputListener listener) {}

    @Override
    public void removeThroughputListener(NetworkQualityThroughputListener listener) {}

    @Override
    public void addRequestFinishedListener(RequestFinishedInfo.Listener listener) {}

    @Override
    public void removeRequestFinishedListener(RequestFinishedInfo.Listener listener) {}

    @Override
    public URLConnection openConnection(URL url) throws IOException {
        return url.openConnection();
    }

    @Override
    public URLConnection openConnection(URL url, Proxy proxy) throws IOException {
        return url.openConnection(proxy);
    }

    @Override
    public URLStreamHandlerFactory createURLStreamHandlerFactory() {
        // Returning null causes this factory to pass though, which ends up using the platform's
        // implementation.
        return new URLStreamHandlerFactory() {
            @Override
            public URLStreamHandler createURLStreamHandler(String protocol) {
                return null;
            }
        };
    }
}
