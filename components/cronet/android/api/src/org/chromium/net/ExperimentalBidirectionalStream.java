<<<<<<< HEAD   (7f0b85 Merge branch 'upstream-import' into upstream-staging)
=======
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * {@link BidirectionalStream} that exposes experimental features. To obtain an instance of this
 * class, cast a {@code BidirectionalStream} to this type. Every instance of {@code
 * BidirectionalStream} can be cast to an instance of this class, as they are backed by the same
 * implementation and hence perform identically. Instances of this class are not meant for general
 * use, but instead only to access experimental features. Experimental features may be deprecated in
 * the future. Use at your own risk.
 *
 * <p>{@hide for consistency with other experimental classes}
 *
 * @deprecated scheduled for deletion, don't use in new code.
 */
@Deprecated
public abstract class ExperimentalBidirectionalStream extends BidirectionalStream {
    /**
     * {@link BidirectionalStream#Builder} that exposes experimental features. To obtain an instance
     * of this class, cast a {@code BidirectionalStream.Builder} to this type. Every instance of
     * {@code BidirectionalStream.Builder} can be cast to an instance of this class, as they are
     * backed by the same implementation and hence perform identically. Instances of this class are
     * not meant for general use, but instead only to access experimental features. Experimental
     * features may be deprecated in the future. Use at your own risk.
     *
     * <p>{@hide for consistency with other experimental classes}
     *
     * @deprecated scheduled for deletion, don't use in new code.
     */
    @Deprecated
    public abstract static class Builder extends BidirectionalStream.Builder {
        /**
         * Binds the request to the specified network handle. Cronet will send this request only
         * using the network associated to this handle. If this network disconnects the request will
         * fail, the exact error will depend on the stage of request processing when the network
         * disconnects. Network handles can be obtained through {@code Network#getNetworkHandle}.
         * Only available starting from Android Marshmallow.
         *
         * @param networkHandle the network handle to bind the request to. Specify {@link
         * ExperimentalCronetEngine#UNBIND_NETWORK_HANDLE} to unbind.
         * @return the builder to facilitate chaining.
         */
        public Builder bindToNetwork(long networkHandle) {
            return this;
        }

        // To support method chaining, override superclass methods to return an
        // instance of this class instead of the parent.

        @Override
        public abstract Builder setHttpMethod(String method);

        @Override
        public abstract Builder addHeader(String header, String value);

        @Override
        public abstract Builder setPriority(int priority);

        @Override
        public abstract Builder delayRequestHeadersUntilFirstFlush(
                boolean delayRequestHeadersUntilFirstFlush);

        @Override
        public Builder addRequestAnnotation(Object annotation) {
            return this;
        }

        @Override
        public Builder setTrafficStatsTag(int tag) {
            return this;
        }

        @Override
        public Builder setTrafficStatsUid(int uid) {
            return this;
        }

        @Override
        public abstract ExperimentalBidirectionalStream build();
    }
}
>>>>>>> BRANCH (3e3c7e Import Cronet version 110.0.5481.154)
