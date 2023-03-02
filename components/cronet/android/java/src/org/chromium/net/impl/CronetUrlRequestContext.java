// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.Network;
import android.net.http.ApiVersion;
import android.os.ConditionVariable;
import android.os.Process;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.UsedByReflection;
import android.net.http.BidirectionalStream;
import android.net.http.HttpEngine;
import org.chromium.net.EffectiveConnectionType;
import android.net.http.ExperimentalBidirectionalStream;
import android.net.http.NetworkQualityRttListener;
import android.net.http.NetworkQualityThroughputListener;
import android.net.http.RequestFinishedInfo;
import org.chromium.net.RttThroughputValues;
import android.net.http.UrlRequest;
import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.CronetLogger.CronetVersion;
import org.chromium.net.urlconnection.CronetHttpURLConnection;
import org.chromium.net.urlconnection.CronetURLStreamHandlerFactory;

import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.time.Instant;
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
    private static final int LOG_NONE = 3; // LOG(FATAL), no VLOG.
    private static final int LOG_DEBUG = -1; // LOG(FATAL...INFO), VLOG(1)
    private static final int LOG_VERBOSE = -2; // LOG(FATAL...INFO), VLOG(2)
    static final String LOG_TAG = CronetUrlRequestContext.class.getSimpleName();

    /**
     * Synchronize access to mUrlRequestContextAdapter and shutdown routine.
     */
    private final Object mLock = new Object();
    private final ConditionVariable mInitCompleted = new ConditionVariable(false);
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

    /** Whether Cronet's logging should be skipped or not. */
    private final boolean mSkipLogging;

    /** The logger to be used for logging. */
    private final CronetLogger mLogger;

    int getCronetEngineId() {
        return mCronetEngineId;
    }

    CronetLogger getCronetLogger() {
        return mLogger;
    }

    @UsedByReflection("CronetEngine.java")
    public CronetUrlRequestContext(final CronetEngineBuilderImpl builder) {
        mCronetEngineId = hashCode();
        mRttListenerList.disableThreadAsserts();
        mThroughputListenerList.disableThreadAsserts();
        mNetworkQualityEstimatorEnabled = builder.networkQualityEstimatorEnabled();
        CronetLibraryLoader.ensureInitialized(builder.getContext(), builder);

        CronetUrlRequestContextJni.get().setMinLogLevel(getLoggingLevel());

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
            mSkipLogging = CronetUrlRequestContextJni.get().skipLogging(
                    mUrlRequestContextAdapter, CronetUrlRequestContext.this);
        }

        if (mSkipLogging) {
            mLogger = CronetLoggerFactory.createNoOpLogger();
        } else {
            mLogger = CronetLoggerFactory.createLogger(builder.getContext(), getCronetSource());
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
        ClassLoader apiClassLoader = HttpEngine.class.getClassLoader();
        ClassLoader implClassLoader = CronetUrlRequest.class.getClassLoader();
        return apiClassLoader.equals(implClassLoader) ? CronetSource.CRONET_SOURCE_STATICALLY_LINKED
                                                      : CronetSource.CRONET_SOURCE_PLAY_SERVICES;
    }

    @VisibleForTesting
    public static long createNativeUrlRequestContextConfig(CronetEngineBuilderImpl builder) {
        final long urlRequestContextConfig =
                CronetUrlRequestContextJni.get().createRequestContextConfig(builder.getUserAgent(),
                        builder.storagePath(), builder.quicEnabled(),
                        builder.getDefaultQuicUserAgentId(), builder.http2Enabled(),
                        builder.brotliEnabled(), builder.cacheDisabled(), builder.httpCacheMode(),
                        builder.httpCacheMaxSize(), builder.experimentalOptions(),
                        builder.mockCertVerifier(), builder.networkQualityEstimatorEnabled(),
                        builder.publicKeyPinningBypassForLocalTrustAnchorsEnabled(),
                        builder.threadPriority(Process.THREAD_PRIORITY_BACKGROUND));
        if (urlRequestContextConfig == 0) {
            throw new IllegalArgumentException("Experimental options parsing failed.");
        }
        for (CronetEngineBuilderImpl.QuicHint quicHint : builder.quicHints()) {
            CronetUrlRequestContextJni.get().addQuicHint(urlRequestContextConfig, quicHint.mHost,
                    quicHint.mPort, quicHint.mAlternatePort);
        }
        for (CronetEngineBuilderImpl.Pkp pkp : builder.publicKeyPins()) {
            CronetUrlRequestContextJni.get().addPkp(urlRequestContextConfig, pkp.mHost, pkp.mHashes,
                    pkp.mIncludeSubdomains, pkp.mExpirationInsant.toEpochMilli());
        }
        return urlRequestContextConfig;
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
            if (mActiveRequestCount.get() != 0) {
                throw new IllegalStateException("Cannot shutdown with active requests.");
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
    public void bindToNetwork(@Nullable Network network) {
        if (network == null) {
            mNetworkHandle = UNBIND_NETWORK_HANDLE;
        } else {
            mNetworkHandle = network.getNetworkHandle();
        }
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
     * Mark request as started to prevent shutdown when there are active
     * requests.
     */
    void onRequestStarted() {
        mActiveRequestCount.incrementAndGet();
    }

    /**
     * Mark request as finished to allow shutdown when there are no active
     * requests.
     */
    void onRequestDestroyed() {
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

    /**
     * @return loggingLevel see {@link #LOG_NONE}, {@link #LOG_DEBUG} and
     *         {@link #LOG_VERBOSE}.
     */
    private int getLoggingLevel() {
        int loggingLevel;
        if (Log.isLoggable(LOG_TAG, Log.VERBOSE)) {
            loggingLevel = LOG_VERBOSE;
        } else if (Log.isLoggable(LOG_TAG, Log.DEBUG)) {
            loggingLevel = LOG_DEBUG;
        } else {
            loggingLevel = LOG_NONE;
        }
        return loggingLevel;
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
        // In integrated mode, network thread is shared from the host.
        // Cronet shouldn't change the property of the thread.
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
                Runnable task = () ->
                        listener.onRttObservation(rttMs, Instant.ofEpochMilli(whenMs), source);
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
                Runnable task = () -> listener.onThroughputObservation(
                        throughputKbps, Instant.ofEpochMilli(whenMs), source);
                postObservationTaskToExecutor(listener.getExecutor(), task);
            }
        }
    }

    void reportRequestFinished(final RequestFinishedInfo requestInfo) {
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
            postObservationTaskToExecutor(listener.getExecutor(), task);
        }
    }

    private static void postObservationTaskToExecutor(Executor executor, Runnable task) {
        try {
            executor.execute(task);
        } catch (RejectedExecutionException failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor",
                    failException);
        }
    }

    public boolean isNetworkThread(Thread thread) {
        return thread == mNetworkThread;
    }

    // Native methods are implemented in cronet_url_request_context_adapter.cc.
    @NativeMethods
    interface Natives {
        long createRequestContextConfig(String userAgent, String storagePath, boolean quicEnabled,
                String quicUserAgentId, boolean http2Enabled, boolean brotliEnabled,
                boolean disableCache, int httpCacheMode, long httpCacheMaxSize,
                String experimentalOptions, long mockCertVerifier,
                boolean enableNetworkQualityEstimator,
                boolean bypassPublicKeyPinningForLocalTrustAnchors, int networkThreadPriority);

        void addQuicHint(long urlRequestContextConfig, String host, int port, int alternatePort);
        void addPkp(long urlRequestContextConfig, String host, byte[][] hashes,
                boolean includeSubdomains, long expirationTime);
        long createRequestContextAdapter(long urlRequestContextConfig);
        int setMinLogLevel(int loggingLevel);
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
        boolean skipLogging(long nativePtr, CronetUrlRequestContext caller);
    }
}