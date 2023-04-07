// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net.impl;

import static android.os.Process.THREAD_PRIORITY_LOWEST;

import android.content.Context;
import android.util.Base64;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import android.net.http.HttpEngine;
import android.net.http.IHttpEngineBuilder;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.IDN;
import java.time.Instant;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Implementation of {@link IHttpEngineBuilder}.
 */
public abstract class CronetEngineBuilderImpl extends IHttpEngineBuilder {
    /**
     * A hint that a host supports QUIC.
     */
    public static class QuicHint {
        // The host.
        final String mHost;
        // Port of the server that supports QUIC.
        final int mPort;
        // Alternate protocol port.
        final int mAlternatePort;

        QuicHint(String host, int port, int alternatePort) {
            mHost = host;
            mPort = port;
            mAlternatePort = alternatePort;
        }
    }

    /**
     * A public key pin.
     */
    public static class Pkp {
        // Host to pin for.
        final String mHost;
        // Array of SHA-256 hashes of keys.
        final byte[][] mHashes;
        // Should pin apply to subdomains?
        final boolean mIncludeSubdomains;
        // When the pin expires.
        final Instant mExpirationInsant;

        Pkp(String host, byte[][] hashes, boolean includeSubdomains, Instant expirationInstant) {
            mHost = host;
            mHashes = hashes;
            mIncludeSubdomains = includeSubdomains;
            mExpirationInsant = expirationInstant;
        }
    }

    /**
     * Mapping between public builder view of HttpCacheMode and internal builder one.
     */
    @VisibleForTesting
    public enum HttpCacheMode {
        DISABLED(HttpCacheType.DISABLED, false),
        DISK(HttpCacheType.DISK, true),
        DISK_NO_HTTP(HttpCacheType.DISK, false),
        MEMORY(HttpCacheType.MEMORY, true);

        private final int mType;
        private final boolean mContentCacheEnabled;

        private HttpCacheMode(int type, boolean contentCacheEnabled) {
            mContentCacheEnabled = contentCacheEnabled;
            mType = type;
        }

        int getType() {
            return mType;
        }

        boolean isContentCacheEnabled() {
            return mContentCacheEnabled;
        }

        @HttpCacheSetting
        @VisibleForTesting
        public int toPublicBuilderCacheMode() {
            switch (this) {
                case DISABLED:
                    return HttpEngine.Builder.HTTP_CACHE_DISABLED;
                case DISK_NO_HTTP:
                    return HttpEngine.Builder.HTTP_CACHE_DISK_NO_HTTP;
                case DISK:
                    return HttpEngine.Builder.HTTP_CACHE_DISK;
                case MEMORY:
                    return HttpEngine.Builder.HTTP_CACHE_IN_MEMORY;
                default:
                    throw new IllegalArgumentException("Unknown internal builder cache mode");
            }
        }

        @VisibleForTesting
        public static HttpCacheMode fromPublicBuilderCacheMode(@HttpCacheSetting int cacheMode) {
            switch (cacheMode) {
                case HttpEngine.Builder.HTTP_CACHE_DISABLED:
                    return DISABLED;
                case HttpEngine.Builder.HTTP_CACHE_DISK_NO_HTTP:
                    return DISK_NO_HTTP;
                case HttpEngine.Builder.HTTP_CACHE_DISK:
                    return DISK;
                case HttpEngine.Builder.HTTP_CACHE_IN_MEMORY:
                    return MEMORY;
                default:
                    throw new IllegalArgumentException("Unknown public builder cache mode");
            }
        }
    }

    private static final Pattern INVALID_PKP_HOST_NAME = Pattern.compile("^[0-9\\.]*$");

    private static final int INVALID_THREAD_PRIORITY = THREAD_PRIORITY_LOWEST + 1;

    // Private fields are simply storage of configuration for the resulting CronetEngine.
    // See setters below for verbose descriptions.
    private final Context mApplicationContext;
    private final List<QuicHint> mQuicHints = new LinkedList<>();
    private final List<Pkp> mPkps = new LinkedList<>();
    private boolean mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
    private String mUserAgent;
    private String mStoragePath;
    private boolean mQuicEnabled;
    private boolean mHttp2Enabled;
    private boolean mBrotiEnabled;
    private boolean mDisableCache;
    private HttpCacheMode mHttpCacheMode;
    private long mHttpCacheMaxSize;
    private String mExperimentalOptions;
    protected long mMockCertVerifier;
    private boolean mNetworkQualityEstimatorEnabled;
    private int mThreadPriority = INVALID_THREAD_PRIORITY;

    /**
     * Default config enables SPDY and QUIC, disables SDCH and HTTP cache.
     * @param context Android {@link Context} for engine to use.
     */
    public CronetEngineBuilderImpl(Context context) {
        mApplicationContext = context.getApplicationContext();
        enableQuic(true);
        enableHttp2(true);
        enableBrotli(false);
        enableHttpCache(HttpEngine.Builder.HTTP_CACHE_DISABLED, 0);
        enableNetworkQualityEstimator(false);
        enablePublicKeyPinningBypassForLocalTrustAnchors(true);
    }

    @Override
    public String getDefaultUserAgent() {
        return UserAgent.getDefault();
    }

    @Override
    public CronetEngineBuilderImpl setUserAgent(String userAgent) {
        mUserAgent = userAgent;
        return this;
    }

    @VisibleForTesting
    public String getUserAgent() {
        return mUserAgent;
    }

    @Override
    public CronetEngineBuilderImpl setStoragePath(String value) {
        if (!new File(value).isDirectory()) {
            throw new IllegalArgumentException("Storage path must be set to existing directory");
        }
        mStoragePath = value;
        return this;
    }

    @VisibleForTesting
    public String storagePath() {
        return mStoragePath;
    }

    @Override
    public CronetEngineBuilderImpl enableQuic(boolean value) {
        mQuicEnabled = value;
        return this;
    }

    @VisibleForTesting
    public boolean quicEnabled() {
        return mQuicEnabled;
    }

    /**
     * Constructs default QUIC User Agent Id string including application name
     * and Cronet version. Returns empty string if QUIC is not enabled.
     *
     * @return QUIC User Agent ID string.
     */
    String getDefaultQuicUserAgentId() {
        return mQuicEnabled ? UserAgent.getDefaultQuicUserAgentId() : "";
    }

    @Override
    public CronetEngineBuilderImpl enableHttp2(boolean value) {
        mHttp2Enabled = value;
        return this;
    }

    @VisibleForTesting
    public boolean http2Enabled() {
        return mHttp2Enabled;
    }

    @Override
    public CronetEngineBuilderImpl enableSdch(boolean value) {
        return this;
    }

    @Override
    public CronetEngineBuilderImpl enableBrotli(boolean value) {
        mBrotiEnabled = value;
        return this;
    }

    @VisibleForTesting
    public boolean brotliEnabled() {
        return mBrotiEnabled;
    }

    @IntDef({HttpEngine.Builder.HTTP_CACHE_DISABLED, HttpEngine.Builder.HTTP_CACHE_IN_MEMORY,
            HttpEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, HttpEngine.Builder.HTTP_CACHE_DISK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HttpCacheSetting {}

    @Override
    public CronetEngineBuilderImpl enableHttpCache(@HttpCacheSetting int cacheMode, long maxSize) {
        HttpCacheMode cacheModeEnum = HttpCacheMode.fromPublicBuilderCacheMode(cacheMode);

        if (cacheModeEnum.getType() == HttpCacheType.DISK && storagePath() == null) {
            throw new IllegalArgumentException("Storage path must be set");
        }

        mHttpCacheMode = cacheModeEnum;
        mHttpCacheMaxSize = maxSize;

        return this;
    }

    boolean cacheDisabled() {
        return !mHttpCacheMode.isContentCacheEnabled();
    }

    long httpCacheMaxSize() {
        return mHttpCacheMaxSize;
    }

    @VisibleForTesting
    public int httpCacheMode() {
        return mHttpCacheMode.getType();
    }

    @HttpCacheSetting
    int publicBuilderHttpCacheMode() {
        return mHttpCacheMode.toPublicBuilderCacheMode();
    }

    @Override
    public CronetEngineBuilderImpl addQuicHint(String host, int port, int alternatePort) {
        if (host.contains("/")) {
            throw new IllegalArgumentException("Illegal QUIC Hint Host: " + host);
        }
        mQuicHints.add(new QuicHint(host, port, alternatePort));
        return this;
    }

    List<QuicHint> quicHints() {
        return mQuicHints;
    }

    @Override
    public CronetEngineBuilderImpl addPublicKeyPins(String hostName, Set<byte[]> pinsSha256,
            boolean includeSubdomains, Instant expirationInstant) {
        if (hostName == null) {
            throw new NullPointerException("The hostname cannot be null");
        }
        if (pinsSha256 == null) {
            throw new NullPointerException("The set of SHA256 pins cannot be null");
        }
        if (expirationInstant == null) {
            throw new NullPointerException("The pin expiration date cannot be null");
        }
        String idnHostName = validateHostNameForPinningAndConvert(hostName);
        // Convert the pin to BASE64 encoding to remove duplicates.
        Map<String, byte[]> hashes = new HashMap<>();
        for (byte[] pinSha256 : pinsSha256) {
            if (pinSha256 == null || pinSha256.length != 32) {
                throw new IllegalArgumentException("Public key pin is invalid");
            }
            hashes.put(Base64.encodeToString(pinSha256, 0), pinSha256);
        }
        // Add new element to PKP list.
        mPkps.add(new Pkp(idnHostName, hashes.values().toArray(new byte[hashes.size()][]),
                includeSubdomains, expirationInstant));
        return this;
    }

    /**
     * Returns list of public key pins.
     * @return list of public key pins.
     */
    List<Pkp> publicKeyPins() {
        return mPkps;
    }

    @Override
    public CronetEngineBuilderImpl enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
        mPublicKeyPinningBypassForLocalTrustAnchorsEnabled = value;
        return this;
    }

    @VisibleForTesting
    public boolean publicKeyPinningBypassForLocalTrustAnchorsEnabled() {
        return mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
    }

    /**
     * Checks whether a given string represents a valid host name for PKP and converts it
     * to ASCII Compatible Encoding representation according to RFC 1122, RFC 1123 and
     * RFC 3490. This method is more restrictive than required by RFC 7469. Thus, a host
     * that contains digits and the dot character only is considered invalid.
     *
     * Note: Currently Cronet doesn't have native implementation of host name validation that
     *       can be used. There is code that parses a provided URL but doesn't ensure its
     *       correctness. The implementation relies on {@code getaddrinfo} function.
     *
     * @param hostName host name to check and convert.
     * @return true if the string is a valid host name.
     * @throws IllegalArgumentException if the the given string does not represent a valid
     *                                  hostname.
     */
    private static String validateHostNameForPinningAndConvert(String hostName)
            throws IllegalArgumentException {
        if (INVALID_PKP_HOST_NAME.matcher(hostName).matches()) {
            throw new IllegalArgumentException("Hostname " + hostName + " is illegal."
                    + " A hostname should not consist of digits and/or dots only.");
        }
        // Workaround for crash, see crbug.com/634914
        if (hostName.length() > 255) {
            throw new IllegalArgumentException("Hostname " + hostName + " is too long."
                    + " The name of the host does not comply with RFC 1122 and RFC 1123.");
        }
        try {
            return IDN.toASCII(hostName, IDN.USE_STD3_ASCII_RULES);
        } catch (IllegalArgumentException ex) {
            throw new IllegalArgumentException("Hostname " + hostName + " is illegal."
                    + " The name of the host does not comply with RFC 1122 and RFC 1123.");
        }
    }

    @Override
    public CronetEngineBuilderImpl setExperimentalOptions(String options) {
        mExperimentalOptions = options;
        return this;
    }

    public String experimentalOptions() {
        return mExperimentalOptions;
    }

    /**
     * Sets a native MockCertVerifier for testing. See
     * {@code MockCertVerifier.createMockCertVerifier} for a method that
     * can be used to create a MockCertVerifier.
     * @param mockCertVerifier pointer to native MockCertVerifier.
     * @return the builder to facilitate chaining.
     */
    @VisibleForTesting
    public CronetEngineBuilderImpl setMockCertVerifierForTesting(long mockCertVerifier) {
        mMockCertVerifier = mockCertVerifier;
        return this;
    }

    long mockCertVerifier() {
        return mMockCertVerifier;
    }

    /**
     * @return true if the network quality estimator has been enabled for
     * this builder.
     */
    @VisibleForTesting
    public boolean networkQualityEstimatorEnabled() {
        return mNetworkQualityEstimatorEnabled;
    }

    @Override
    public CronetEngineBuilderImpl enableNetworkQualityEstimator(boolean value) {
        mNetworkQualityEstimatorEnabled = value;
        return this;
    }

    @Override
    public CronetEngineBuilderImpl setThreadPriority(int priority) {
        if (priority > THREAD_PRIORITY_LOWEST || priority < -20) {
            throw new IllegalArgumentException("Thread priority invalid");
        }
        mThreadPriority = priority;
        return this;
    }

    /**
     * @return thread priority provided by user, or {@code defaultThreadPriority} if none provided.
     */
    @VisibleForTesting
    public int threadPriority(int defaultThreadPriority) {
        return mThreadPriority == INVALID_THREAD_PRIORITY ? defaultThreadPriority : mThreadPriority;
    }

    /**
     * Returns {@link Context} for builder.
     *
     * @return {@link Context} for builder.
     */
    Context getContext() {
        return mApplicationContext;
    }
}
