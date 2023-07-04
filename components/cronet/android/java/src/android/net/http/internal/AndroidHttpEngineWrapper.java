package android.net.http;

import org.chromium.net.ExperimentalCronetEngine;
import java.io.IOException;
import java.net.URL;
import java.net.URLConnection;
import android.net.Network;
import java.net.URLStreamHandlerFactory;
import java.util.concurrent.Executor;

import android.net.http.HttpEngine;

class AndroidHttpEngineWrapper extends HttpEngine {

  private final ExperimentalCronetEngine delegate;

  public AndroidHttpEngineWrapper(ExperimentalCronetEngine delegate) {
    this.delegate = delegate;
  }

  @Override
  public void shutdown() {
    delegate.shutdown();
  }

  @Override
  public URLConnection openConnection(URL url) throws IOException {
    return CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
        () -> delegate.openConnection(url), IOException.class);
  }

  @Override
  public URLStreamHandlerFactory createUrlStreamHandlerFactory() {
    return delegate.createURLStreamHandlerFactory();
  }

  @Override
  public void bindToNetwork(Network network) {
    long networkHandle = delegate.UNBIND_NETWORK_HANDLE;
    if (network != null) {
      networkHandle = network.getNetworkHandle();
    }
    delegate.bindToNetwork(networkHandle);
  }


  @Override
  public android.net.http.BidirectionalStream.Builder newBidirectionalStreamBuilder(
      String url, Executor executor, android.net.http.BidirectionalStream.Callback callback) {
    return new AndroidBidirectionalStreamBuilderWrapper(
        delegate.newBidirectionalStreamBuilder(
            url, new AndroidBidirectionalStreamCallbackWrapper(callback), executor));
  }

  @Override
  public android.net.http.UrlRequest.Builder newUrlRequestBuilder(
      String url, Executor executor, android.net.http.UrlRequest.Callback callback) {
    return new AndroidUrlRequestBuilderWrapper(
        delegate.newUrlRequestBuilder(url, new AndroidUrlRequestCallbackWrapper(callback), executor));
  }
}
