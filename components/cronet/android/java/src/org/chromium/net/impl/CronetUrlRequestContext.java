// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import org.chromium.net.ApiVersion;
import android.os.Build;
import android.os.ConditionVariable;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetEngine;
import org.chromium.net.EffectiveConnectionType;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.NetworkQualityRttListener;
import org.chromium.net.NetworkQualityThroughputListener;
import org.chromium.net.RequestContextConfigOptions;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.RttThroughputValues;
import org.chromium.net.UrlRequest;
import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.CronetLogger.CronetVersion;
import org.chromium.net.urlconnection.CronetHttpURLConnection;
import org.chromium.net.urlconnection.CronetURLStreamHandlerFactory;

import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.GuardedBy;

/**
 * CronetEngine using Chromium HTTP stack implementation.
 */
@JNINamespace("cronet")
@UsedByReflection("CronetEngine.java")
@VisibleForTesting
public class CronetUrlRequestContext extends CronetEngineBase {
    static final String LOG_TAG = CronetUrlRequestContext.class.getSimpleName();

    /**
     * Synchronize access to mUrlRequestContextAdapter and shutdown routine.
     */
    private final Object mLock = new Object();
    private final ConditionVariable mInitCompleted = new ConditionVariable(false);

    /**
     * The number of started requests where the terminal callback (i.e.
     * onSucceeded/onCancelled/onFailed) has not yet been called.
     */
    private final AtomicInteger mRunningRequestCount = new AtomicInteger(0);
    /*
     * The number of started requests where the terminal callbacks (i.e.
     * onSucceeded/onCancelled/onFailed, request finished listeners) have not
     * all returned yet.
     *
     * By definition this is always greater than or equal to
     * mRunningRequestCount. The difference between the two is the number of
     * terminal callbacks that are currently running.
     */
    private final AtomicInteger mActiveRequestCount = new AtomicInteger(0);

    @GuardedBy("mLock")
    private long mUrlRequestContextAdapter;
    /**
     * This field is accessed without synchronization, but only for the purposes of reference
     * equality comparison with other threads. If such a comparison is performed on the network
     * thread, then there is a happens-before edge between the write of this field and the
     * subsequent read; if it's performed on another thread, then observing a value of null won't
     * change the result of the comparison.
     */
    private Thread mNetworkThread;

    private final boolean mNetworkQualityEstimatorEnabled;

    /**
     * Locks operations on network quality listeners, because listener
     * addition and removal may occur on a different thread from notification.
     */
    private final Object mNetworkQualityLock = new Object();

    /**
     * Locks operations on the list of RequestFinishedInfo.Listeners, because operations can happen
     * on any thread. This should be used for fine-grained locking only. In particular, don't call
     * any UrlRequest methods that acquire mUrlRequestAdapterLock while holding this lock.
     */
    private final Object mFinishedListenerLock = new Object();

    /**
     * Current effective connection type as computed by the network quality
     * estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mEffectiveConnectionType = EffectiveConnectionType.TYPE_UNKNOWN;

    /**
     * Current estimate of the HTTP RTT (in milliseconds) computed by the
     * network quality estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mHttpRttMs = RttThroughputValues.INVALID_RTT_THROUGHPUT;

    /**
     * Current estimate of the transport RTT (in milliseconds) computed by the
     * network quality estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mTransportRttMs = RttThroughputValues.INVALID_RTT_THROUGHPUT;

    /**
     * Current estimate of the downstream throughput (in kilobits per second)
     * computed by the network quality estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mDownstreamThroughputKbps = RttThroughputValues.INVALID_RTT_THROUGHPUT;

    @GuardedBy("mNetworkQualityLock")
    private final ObserverList<VersionSafeCallbacks.NetworkQualityRttListenerWrapper>
            mRttListenerList =
                    new ObserverList<VersionSafeCallbacks.NetworkQualityRttListenerWrapper>();

    @GuardedBy("mNetworkQualityLock")
    private final ObserverList<VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper>
            mThroughputListenerList =
                    new ObserverList<VersionSafeCallbacks
                                             .NetworkQualityThroughputListenerWrapper>();

    @GuardedBy("mFinishedListenerLock")
    private final Map<RequestFinishedInfo.Listener,
            VersionSafeCallbacks.RequestFinishedInfoListener> mFinishedListenerMap =
            new HashMap<RequestFinishedInfo.Listener,
                    VersionSafeCallbacks.RequestFinishedInfoListener>();

    private final ConditionVariable mStopNetLogCompleted = new ConditionVariable();

    /** Set of storage paths currently in use. */
    @GuardedBy("sInUseStoragePaths")
    private static final HashSet<String> sInUseStoragePaths = new HashSet<String>();

    /** Storage path used by this context. */
    private final String mInUseStoragePath;

    /**
     * True if a NetLog observer is active.
     */
    @GuardedBy("mLock")
    private boolean mIsLogging;

    /**
     * True if NetLog is being shutdown.
     */
    @GuardedBy("mLock")
    private boolean mIsStoppingNetLog;

    /** The network handle to be used for requests that do not explicitly specify one. **/
    private long mNetworkHandle = DEFAULT_NETWORK_HANDLE;

    private final int mCronetEngineId;

    /** Whether Cronet Telemetry should be enabled or not. */
    private final boolean mEnableTelemetry;

    /** The logger to be used for logging. */
    private final CronetLogger mLogger;

    int getCronetEngineId() {
        return mCronetEngineId;
    }

    CronetLogger getCronetLogger() {
        return mLogger;
    }

    public boolean getEnableTelemetryForTesting() {
        return mEnableTelemetry;
    }

    @UsedByReflection("CronetEngine.java")
    public CronetUrlRequestContext(final CronetEngineBuilderImpl builder) {
        mCronetEngineId = hashCode();
        mRttListenerList.disableThreadAsserts();
        mThroughputListenerList.disableThreadAsserts();
        mNetworkQualityEstimatorEnabled = builder.networkQualityEstimatorEnabled();
        CronetLibraryLoader.ensureInitialized(builder.getContext(), builder);
        if (builder.httpCacheMode() == HttpCacheType.DISK) {
            mInUseStoragePath = builder.storagePath();
            synchronized (sInUseStoragePaths) {
                if (!sInUseStoragePaths.add(mInUseStoragePath)) {
                    throw new IllegalStateException("Disk cache storage path already in use");
                }
            }
        } else {
            mInUseStoragePath = null;
        }
        synchronized (mLock) {
            mUrlRequestContextAdapter =
                    CronetUrlRequestContextJni.get().createRequestContextAdapter(
                            createNativeUrlRequestContextConfig(builder));
            if (mUrlRequestContextAdapter == 0) {
                throw new NullPointerException("Context Adapter creation failed.");
            }
            mEnableTelemetry = CronetUrlRequestContextJni.get().getEnableTelemetry(
                    mUrlRequestContextAdapter, CronetUrlRequestContext.this);
        }

        if (mEnableTelemetry) {
            mLogger = CronetLoggerFactory.createLogger(builder.getContext(), getCronetSource());
        } else {
            mLogger = CronetLoggerFactory.createNoOpLogger();
        }
        try {
            mLogger.logCronetEngineCreation(getCronetEngineId(),
                    new CronetEngineBuilderInfo(builder), buildCronetVersion(), getCronetSource());
        } catch (RuntimeException e) {
            // Handle any issue gracefully, we should never crash due failures while logging.
            Log.e(LOG_TAG, "Error while trying to log CronetEngine creation: ", e);
        }

        // Init native Chromium URLRequestContext on init thread.
        CronetLibraryLoader.postToInitThread(new Runnable() {
            @Override
            public void run() {
                CronetLibraryLoader.ensureInitializedOnInitThread();
                synchronized (mLock) {
                    // mUrlRequestContextAdapter is guaranteed to exist until
                    // initialization on init and network threads completes and
                    // initNetworkThread is called back on network thread.
                    CronetUrlRequestContextJni.get().initRequestContextOnInitThread(
                            mUrlRequestContextAdapter, CronetUrlRequestContext.this);
                }
            }
        });
    }

    static CronetSource getCronetSource() {
        ClassLoader implClassLoader = CronetUrlRequest.class.getClassLoader();
        if (implClassLoader.toString().startsWith("java.lang.BootClassLoader")) {
            return CronetSource.CRONET_SOURCE_PLATFORM;
        }
        ClassLoader apiClassLoader = CronetEngine.class.getClassLoader();
        return apiClassLoader.equals(implClassLoader) ? CronetSource.CRONET_SOURCE_STATICALLY_LINKED
                                                      : CronetSource.CRONET_SOURCE_PLAY_SERVICES;
    }

    @VisibleForTesting
    public static long createNativeUrlRequestContextConfig(CronetEngineBuilderImpl builder) {
        final long urlRequestContextConfig =
                CronetUrlRequestContextJni.get().createRequestContextConfig(
                        createRequestContextConfigOptions(builder).toByteArray());
        if (urlRequestContextConfig == 0) {
            throw new IllegalArgumentException("Experimental options parsing failed.");
        }
        for (CronetEngineBuilderImpl.QuicHint quicHint : builder.quicHints()) {
            CronetUrlRequestContextJni.get().addQuicHint(urlRequestContextConfig, quicHint.mHost,
                    quicHint.mPort, quicHint.mAlternatePort);
        }
        for (CronetEngineBuilderImpl.Pkp pkp : builder.publicKeyPins()) {
            CronetUrlRequestContextJni.get().addPkp(urlRequestContextConfig, pkp.mHost, pkp.mHashes,
                    pkp.mIncludeSubdomains, pkp.mExpirationDate.getTime());
        }
        return urlRequestContextConfig;
    }

    private static RequestContextConfigOptions createRequestContextConfigOptions(
            CronetEngineBuilderImpl engineBuilder) {
        RequestContextConfigOptions.Builder resultBuilder =
                RequestContextConfigOptions.newBuilder()
                        .setQuicEnabled(engineBuilder.quicEnabled())
                        .setHttp2Enabled(engineBuilder.http2Enabled())
                        .setBrotliEnabled(engineBuilder.brotliEnabled())
                        .setDisableCache(engineBuilder.cacheDisabled())
                        .setHttpCacheMode(engineBuilder.httpCacheMode())
                        .setHttpCacheMaxSize(engineBuilder.httpCacheMaxSize())
                        .setMockCertVerifier(engineBuilder.mockCertVerifier())
                        .setEnableNetworkQualityEstimator(
                                engineBuilder.networkQualityEstimatorEnabled())
                        .setBypassPublicKeyPinningForLocalTrustAnchors(
                                engineBuilder.publicKeyPinningBypassForLocalTrustAnchorsEnabled())
                        .setNetworkThreadPriority(
                                engineBuilder.threadPriority(Process.THREAD_PRIORITY_BACKGROUND));

        if (engineBuilder.getUserAgent() != null) {
            resultBuilder.setUserAgent(engineBuilder.getUserAgent());
        }

        if (engineBuilder.storagePath() != null) {
            resultBuilder.setStoragePath(engineBuilder.storagePath());
        }

        if (engineBuilder.getDefaultQuicUserAgentId() != null) {
            resultBuilder.setQuicDefaultUserAgentId(engineBuilder.getDefaultQuicUserAgentId());
        }

        if (engineBuilder.experimentalOptions() != null) {
            resultBuilder.setExperimentalOptions(engineBuilder.experimentalOptions());
        }

        return resultBuilder.build();
    }

    @Override
    public ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, BidirectionalStream.Callback callback, Executor executor) {
        return new BidirectionalStreamBuilderImpl(url, callback, executor, this);
    }

    @Override
    public UrlRequestBase createRequest(String url, UrlRequest.Callback callback, Executor executor,
            int priority, Collection<Object> requestAnnotations, boolean disableCache,
            boolean disableConnectionMigration, boolean allowDirectExecutor,
            boolean trafficStatsTagSet, int trafficStatsTag, boolean trafficStatsUidSet,
            int trafficStatsUid, RequestFinishedInfo.Listener requestFinishedListener,
            int idempotency, long networkHandle) {
        // if this request is not bound to network, use the network bound to the engine.
        if (networkHandle == DEFAULT_NETWORK_HANDLE) {
            networkHandle = mNetworkHandle;
        }
        synchronized (mLock) {
            checkHaveAdapter();
            return new CronetUrlRequest(this, url, priority, callback, executor, requestAnnotations,
                    disableCache, disableConnectionMigration, allowDirectExecutor,
                    trafficStatsTagSet, trafficStatsTag, trafficStatsUidSet, trafficStatsUid,
                    requestFinishedListener, idempotency, networkHandle);
        }
    }

    @Override
    protected ExperimentalBidirectionalStream createBidirectionalStream(String url,
            BidirectionalStream.Callback callback, Executor executor, String httpMethod,
            List<Map.Entry<String, String>> requestHeaders, @StreamPriority int priority,
            boolean delayRequestHeadersUntilFirstFlush, Collection<Object> requestAnnotations,
            boolean trafficStatsTagSet, int trafficStatsTag, boolean trafficStatsUidSet,
            int trafficStatsUid, long networkHandle) {
        if (networkHandle == DEFAULT_NETWORK_HANDLE) {
            networkHandle = mNetworkHandle;
        }
        synchronized (mLock) {
            checkHaveAdapter();
            return new CronetBidirectionalStream(this, url, priority, callback, executor,
                    httpMethod, requestHeaders, delayRequestHeadersUntilFirstFlush,
                    requestAnnotations, trafficStatsTagSet, trafficStatsTag, trafficStatsUidSet,
                    trafficStatsUid, networkHandle);
        }
    }

    @Override
    public String getVersionString() {
        return "Cronet/" + ImplVersion.getCronetVersionWithLastChange();
    }

    @Override
    public int getActiveRequestCount() {
        return mActiveRequestCount.get();
    }

    private CronetVersion buildCronetVersion() {
        return new CronetVersion(ApiVersion.getCronetVersion());
    }

    @Override
    public void shutdown() {
        if (mInUseStoragePath != null) {
            synchronized (sInUseStoragePaths) {
                sInUseStoragePaths.remove(mInUseStoragePath);
            }
        }
        synchronized (mLock) {
            checkHaveAdapter();
            if (mRunningRequestCount.get() != 0) {
                throw new IllegalStateException("Cannot shutdown with running requests.");
            }
            // Destroying adapter stops the network thread, so it cannot be
            // called on network thread.
            if (Thread.currentThread() == mNetworkThread) {
                throw new IllegalThreadStateException("Cannot shutdown from network thread.");
            }
        }
        // Wait for init to complete on init and network thread (without lock,
        // so other thread could access it).
        mInitCompleted.block();

        // If not logging, this is a no-op.
        stopNetLog();

        synchronized (mLock) {
            // It is possible that adapter is already destroyed on another thread.
            if (!haveRequestContextAdapter()) {
                return;
            }
            CronetUrlRequestContextJni.get().destroy(
                    mUrlRequestContextAdapter, CronetUrlRequestContext.this);
            mUrlRequestContextAdapter = 0;
        }
    }

    @Override
    public void startNetLogToFile(String fileName, boolean logAll) {
        synchronized (mLock) {
            checkHaveAdapter();
            if (mIsLogging) {
                return;
            }
            if (!CronetUrlRequestContextJni.get().startNetLogToFile(mUrlRequestContextAdapter,
                        CronetUrlRequestContext.this, fileName, logAll)) {
                throw new RuntimeException("Unable to start NetLog");
            }
            mIsLogging = true;
        }
    }

    @Override
    public void startNetLogToDisk(String dirPath, boolean logAll, int maxSize) {
        synchronized (mLock) {
            checkHaveAdapter();
            if (mIsLogging) {
                return;
            }
            CronetUrlRequestContextJni.get().startNetLogToDisk(mUrlRequestContextAdapter,
                    CronetUrlRequestContext.this, dirPath, logAll, maxSize);
            mIsLogging = true;
        }
    }

    @Override
    public void stopNetLog() {
        synchronized (mLock) {
            checkHaveAdapter();
            if (!mIsLogging || mIsStoppingNetLog) {
                return;
            }
            CronetUrlRequestContextJni.get().stopNetLog(
                    mUrlRequestContextAdapter, CronetUrlRequestContext.this);
            mIsStoppingNetLog = true;
        }
        mStopNetLogCompleted.block();
        mStopNetLogCompleted.close();
        synchronized (mLock) {
            mIsStoppingNetLog = false;
            mIsLogging = false;
        }
    }

    @CalledByNative
    public void stopNetLogCompleted() {
        mStopNetLogCompleted.open();
    }

    // This method is intentionally non-static to ensure Cronet native library
    // is loaded by class constructor.
    @Override
    public byte[] getGlobalMetricsDeltas() {
        return CronetUrlRequestContextJni.get().getHistogramDeltas();
    }

    @Override
    public int getEffectiveConnectionType() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return convertConnectionTypeToApiValue(mEffectiveConnectionType);
        }
    }

    @Override
    public int getHttpRttMs() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return mHttpRttMs != RttThroughputValues.INVALID_RTT_THROUGHPUT
                    ? mHttpRttMs
                    : CONNECTION_METRIC_UNKNOWN;
        }
    }

    @Override
    public int getTransportRttMs() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return mTransportRttMs != RttThroughputValues.INVALID_RTT_THROUGHPUT
                    ? mTransportRttMs
                    : CONNECTION_METRIC_UNKNOWN;
        }
    }

    @Override
    public int getDownstreamThroughputKbps() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return mDownstreamThroughputKbps != RttThroughputValues.INVALID_RTT_THROUGHPUT
                    ? mDownstreamThroughputKbps
                    : CONNECTION_METRIC_UNKNOWN;
        }
    }

    @Override
    public void bindToNetwork(long networkHandle) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            throw new UnsupportedOperationException(
                    "The multi-network API is available starting from Android Marshmallow");
        }
        mNetworkHandle = networkHandle;
    }

    @VisibleForTesting
    @Override
    public void configureNetworkQualityEstimatorForTesting(boolean useLocalHostRequests,
            boolean useSmallerResponses, boolean disableOfflineCheck) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mLock) {
            checkHaveAdapter();
            CronetUrlRequestContextJni.get().configureNetworkQualityEstimatorForTesting(
                    mUrlRequestContextAdapter, CronetUrlRequestContext.this, useLocalHostRequests,
                    useSmallerResponses, disableOfflineCheck);
        }
    }

    @Override
    public void addRttListener(NetworkQualityRttListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mRttListenerList.isEmpty()) {
                synchronized (mLock) {
                    checkHaveAdapter();
                    CronetUrlRequestContextJni.get().provideRTTObservations(
                            mUrlRequestContextAdapter, CronetUrlRequestContext.this, true);
                }
            }
            mRttListenerList.addObserver(
                    new VersionSafeCallbacks.NetworkQualityRttListenerWrapper(listener));
        }
    }

    @Override
    public void removeRttListener(NetworkQualityRttListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mRttListenerList.removeObserver(
                        new VersionSafeCallbacks.NetworkQualityRttListenerWrapper(listener))) {
                if (mRttListenerList.isEmpty()) {
                    synchronized (mLock) {
                        checkHaveAdapter();
                        CronetUrlRequestContextJni.get().provideRTTObservations(
                                mUrlRequestContextAdapter, CronetUrlRequestContext.this, false);
                    }
                }
            }
        }
    }

    @Override
    public void addThroughputListener(NetworkQualityThroughputListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mThroughputListenerList.isEmpty()) {
                synchronized (mLock) {
                    checkHaveAdapter();
                    CronetUrlRequestContextJni.get().provideThroughputObservations(
                            mUrlRequestContextAdapter, CronetUrlRequestContext.this, true);
                }
            }
            mThroughputListenerList.addObserver(
                    new VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper(listener));
        }
    }

    @Override
    public void removeThroughputListener(NetworkQualityThroughputListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mThroughputListenerList.removeObserver(
                        new VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper(
                                listener))) {
                if (mThroughputListenerList.isEmpty()) {
                    synchronized (mLock) {
                        checkHaveAdapter();
                        CronetUrlRequestContextJni.get().provideThroughputObservations(
                                mUrlRequestContextAdapter, CronetUrlRequestContext.this, false);
                    }
                }
            }
        }
    }

    @Override
    public void addRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        synchronized (mFinishedListenerLock) {
            mFinishedListenerMap.put(
                    listener, new VersionSafeCallbacks.RequestFinishedInfoListener(listener));
        }
    }

    @Override
    public void removeRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        synchronized (mFinishedListenerLock) {
            mFinishedListenerMap.remove(listener);
        }
    }

    boolean hasRequestFinishedListener() {
        synchronized (mFinishedListenerLock) {
            return !mFinishedListenerMap.isEmpty();
        }
    }

    @Override
    public URLConnection openConnection(URL url) {
        return openConnection(url, Proxy.NO_PROXY);
    }

    @Override
    public URLConnection openConnection(URL url, Proxy proxy) {
        if (proxy.type() != Proxy.Type.DIRECT) {
            throw new UnsupportedOperationException();
        }
        String protocol = url.getProtocol();
        if ("http".equals(protocol) || "https".equals(protocol)) {
            return new CronetHttpURLConnection(url, this);
        }
        throw new UnsupportedOperationException("Unexpected protocol:" + protocol);
    }

    @Override
    public URLStreamHandlerFactory createURLStreamHandlerFactory() {
        return new CronetURLStreamHandlerFactory(this);
    }

    /**
     * Mark request as started for the purposes of getActiveRequestCount(), and
     * to prevent shutdown when there are running requests.
     */
    void onRequestStarted() {
        mActiveRequestCount.incrementAndGet();
        mRunningRequestCount.incrementAndGet();
    }

    /**
     * Mark request as destroyed to allow shutdown when there are no running
     * requests. Should be called *before* the terminal callback is called, so
     * that users can call shutdown() from the terminal callback.
     */
    void onRequestDestroyed() {
        mRunningRequestCount.decrementAndGet();
    }

    /**
     * Mark request as finished for the purposes of getActiveRequestCount().
     * Should be called *after* the terminal callback returns.
     */
    void onRequestFinished() {
        mActiveRequestCount.decrementAndGet();
    }

    @VisibleForTesting
    public long getUrlRequestContextAdapter() {
        synchronized (mLock) {
            checkHaveAdapter();
            return mUrlRequestContextAdapter;
        }
    }

    @GuardedBy("mLock")
    private void checkHaveAdapter() throws IllegalStateException {
        if (!haveRequestContextAdapter()) {
            throw new IllegalStateException("Engine is shut down.");
        }
    }

    @GuardedBy("mLock")
    private boolean haveRequestContextAdapter() {
        return mUrlRequestContextAdapter != 0;
    }

    private static int convertConnectionTypeToApiValue(@EffectiveConnectionType int type) {
        switch (type) {
            case EffectiveConnectionType.TYPE_OFFLINE:
                return EFFECTIVE_CONNECTION_TYPE_OFFLINE;
            case EffectiveConnectionType.TYPE_SLOW_2G:
                return EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
            case EffectiveConnectionType.TYPE_2G:
                return EFFECTIVE_CONNECTION_TYPE_2G;
            case EffectiveConnectionType.TYPE_3G:
                return EFFECTIVE_CONNECTION_TYPE_3G;
            case EffectiveConnectionType.TYPE_4G:
                return EFFECTIVE_CONNECTION_TYPE_4G;
            case EffectiveConnectionType.TYPE_UNKNOWN:
                return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
            default:
                throw new RuntimeException(
                        "Internal Error: Illegal EffectiveConnectionType value " + type);
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void initNetworkThread() {
        mNetworkThread = Thread.currentThread();
        mInitCompleted.open();
        Thread.currentThread().setName("ChromiumNet");
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onEffectiveConnectionTypeChanged(int effectiveConnectionType) {
        synchronized (mNetworkQualityLock) {
            // Convert the enum returned by the network quality estimator to an enum of type
            // EffectiveConnectionType.
            mEffectiveConnectionType = effectiveConnectionType;
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRTTOrThroughputEstimatesComputed(
            final int httpRttMs, final int transportRttMs, final int downstreamThroughputKbps) {
        synchronized (mNetworkQualityLock) {
            mHttpRttMs = httpRttMs;
            mTransportRttMs = transportRttMs;
            mDownstreamThroughputKbps = downstreamThroughputKbps;
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRttObservation(final int rttMs, final long whenMs, final int source) {
        synchronized (mNetworkQualityLock) {
            for (final VersionSafeCallbacks.NetworkQualityRttListenerWrapper listener :
                    mRttListenerList) {
                Runnable task = new Runnable() {
                    @Override
                    public void run() {
                        listener.onRttObservation(rttMs, whenMs, source);
                    }
                };
                postObservationTaskToExecutor(listener.getExecutor(), task);
            }
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onThroughputObservation(
            final int throughputKbps, final long whenMs, final int source) {
        synchronized (mNetworkQualityLock) {
            for (final VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper listener :
                    mThroughputListenerList) {
                Runnable task = new Runnable() {
                    @Override
                    public void run() {
                        listener.onThroughputObservation(throughputKbps, whenMs, source);
                    }
                };
                postObservationTaskToExecutor(listener.getExecutor(), task);
            }
        }
    }

    void reportRequestFinished(
            final RequestFinishedInfo requestInfo, RefCountDelegate inflightCallbackCount) {
        ArrayList<VersionSafeCallbacks.RequestFinishedInfoListener> currentListeners;
        synchronized (mFinishedListenerLock) {
            if (mFinishedListenerMap.isEmpty()) return;
            currentListeners = new ArrayList<VersionSafeCallbacks.RequestFinishedInfoListener>(
                    mFinishedListenerMap.values());
        }
        for (final VersionSafeCallbacks.RequestFinishedInfoListener listener : currentListeners) {
            Runnable task = new Runnable() {
                @Override
                public void run() {
                    listener.onRequestFinished(requestInfo);
                }
            };
            postObservationTaskToExecutor(listener.getExecutor(), task, inflightCallbackCount);
        }
    }

    private static void postObservationTaskToExecutor(
            Executor executor, Runnable task, RefCountDelegate inflightCallbackCount) {
        if (inflightCallbackCount != null) inflightCallbackCount.increment();
        try {
            executor.execute(() -> {
                try {
                    task.run();
                } catch (Exception e) {
                    Log.e(LOG_TAG, "Exception thrown from observation task", e);
                } finally {
                    if (inflightCallbackCount != null) inflightCallbackCount.decrement();
                }
            });
        } catch (RejectedExecutionException failException) {
            if (inflightCallbackCount != null) inflightCallbackCount.decrement();
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor",
                    failException);
        }
    }

    private static void postObservationTaskToExecutor(Executor executor, Runnable task) {
        postObservationTaskToExecutor(executor, task, null);
    }

    public boolean isNetworkThread(Thread thread) {
        return thread == mNetworkThread;
    }

    // Native methods are implemented in cronet_url_request_context_adapter.cc.
    @NativeMethods
    interface Natives {
        long createRequestContextConfig(byte[] serializedRequestContextConfigOptions);

        void addQuicHint(long urlRequestContextConfig, String host, int port, int alternatePort);
        void addPkp(long urlRequestContextConfig, String host, byte[][] hashes,
                boolean includeSubdomains, long expirationTime);
        long createRequestContextAdapter(long urlRequestContextConfig);
        byte[] getHistogramDeltas();
        @NativeClassQualifiedName("CronetContextAdapter")
        void destroy(long nativePtr, CronetUrlRequestContext caller);

        @NativeClassQualifiedName("CronetContextAdapter")
        boolean startNetLogToFile(
                long nativePtr, CronetUrlRequestContext caller, String fileName, boolean logAll);

        @NativeClassQualifiedName("CronetContextAdapter")
        void startNetLogToDisk(long nativePtr, CronetUrlRequestContext caller, String dirPath,
                boolean logAll, int maxSize);

        @NativeClassQualifiedName("CronetContextAdapter")
        void stopNetLog(long nativePtr, CronetUrlRequestContext caller);

        @NativeClassQualifiedName("CronetContextAdapter")
        void initRequestContextOnInitThread(long nativePtr, CronetUrlRequestContext caller);

        @NativeClassQualifiedName("CronetContextAdapter")
        void configureNetworkQualityEstimatorForTesting(long nativePtr,
                CronetUrlRequestContext caller, boolean useLocalHostRequests,
                boolean useSmallerResponses, boolean disableOfflineCheck);

        @NativeClassQualifiedName("CronetContextAdapter")
        void provideRTTObservations(long nativePtr, CronetUrlRequestContext caller, boolean should);

        @NativeClassQualifiedName("CronetContextAdapter")
        void provideThroughputObservations(
                long nativePtr, CronetUrlRequestContext caller, boolean should);

        @NativeClassQualifiedName("CronetContextAdapter")
        boolean getEnableTelemetry(long nativePtr, CronetUrlRequestContext caller);
    }
}