// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.net.http.ConnectionMigrationOptions;
import android.net.http.ExperimentalHttpEngine;

import androidx.annotation.VisibleForTesting;

import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;

/**
 * An implementation of {@link URLStreamHandlerFactory} to handle HTTP and HTTPS
 * traffic. An instance of this class can be installed via
 * {@link java.net.URL#setURLStreamHandlerFactory} thus using Cronet by default for all requests
 * created via {@link java.net.URL#openConnection}.
 * <p>
 * Cronet does not use certain HTTP features provided via the system:
 * <ul>
 * <li>the HTTP cache installed via
 *     {@link android.net.http.HttpResponseCache#install}</li>
 * <li>the HTTP authentication method installed via
 *     {@link java.net.Authenticator#setDefault}</li>
 * <li>the HTTP cookie storage installed via {@link java.net.CookieHandler#setDefault}</li>
 * </ul>
 * <p>
 * While Cronet supports and encourages requests using the HTTPS protocol,
 * Cronet does not provide support for the
 * {@link javax.net.ssl.HttpsURLConnection} API. This lack of support also
 * includes not using certain HTTPS features provided via the system:
 * <ul>
 * <li>the HTTPS hostname verifier installed via {@link
 *     javax.net.ssl.HttpsURLConnection#setDefaultHostnameVerifier(javax.net.ssl.HostnameVerifier)
 *     HttpsURLConnection.setDefaultHostnameVerifier(javax.net.ssl.HostnameVerifier)}</li>
 * <li>the HTTPS socket factory installed via {@link
 *     javax.net.ssl.HttpsURLConnection#setDefaultSSLSocketFactory(javax.net.ssl.SSLSocketFactory)
 *     HttpsURLConnection.setDefaultSSLSocketFactory(javax.net.ssl.SSLSocketFactory)}</li>
 * </ul>
 *
 * {@hide}
 */
public class CronetURLStreamHandlerFactory implements URLStreamHandlerFactory {
    private ExperimentalHttpEngine mCronetEngine;
    private CronetHttpURLStreamHandler mHandler;

    /**
     * Creates a {@link CronetURLStreamHandlerFactory} to handle HTTP and HTTPS
     * traffic.
     * @param cronetEngine the {@link ExperimentalHttpEngine} to be used.
     * @throws NullPointerException if config is null.
     */
    public CronetURLStreamHandlerFactory(ExperimentalHttpEngine cronetEngine) {
        if (cronetEngine == null) {
            throw new NullPointerException("CronetEngine is null.");
        }
        mCronetEngine = cronetEngine;
    }

    /**
     * Returns a {@link CronetHttpURLStreamHandler} for HTTP and HTTPS, and
     * {@code null} for other protocols.
     */
    @Override
    public URLStreamHandler createURLStreamHandler(String protocol) {
        if ("http".equals(protocol) || "https".equals(protocol)) {
            // only return cached handler for the supported protocol
            if (mHandler != null) return mHandler;
            mHandler = new CronetHttpURLStreamHandler(mCronetEngine);
            return mHandler;
        }
        return null;
    }

    @VisibleForTesting
    public void swapCronetEngineForTesting(
            ExperimentalHttpEngine httpEngine) {
        if (mHandler != null) {
            mHandler.swapCronetEngineForTesting(httpEngine);
            return;
        }
        mCronetEngine = httpEngine;
    }
}
