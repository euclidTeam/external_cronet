package android.net.http;

import static android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_UNSPECIFIED;
import static android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_ENABLED;
import static android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_DISABLED;
import static android.net.http.DnsOptions.DNS_OPTION_UNSPECIFIED;
import static android.net.http.DnsOptions.DNS_OPTION_ENABLED;
import static android.net.http.DnsOptions.DNS_OPTION_DISABLED;

import org.json.JSONException;
import org.json.JSONObject;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import android.util.Log;
import java.util.Date;
import java.time.Instant;
import java.util.Set;
import org.chromium.net.ExperimentalCronetEngine;
import android.net.http.HttpEngine;

public class EngineBuilderWrapper extends IHttpEngineBuilder {

  private static final String TAG = "HttpEngineBuilderWrap";

  private final List<ExperimentalOptionsPatch> mExperimentalOptionsPatches = new ArrayList<>();
  private JSONObject mParsedExperimentalOptions = new JSONObject();
  private final ExperimentalCronetEngine.Builder backend;

  public EngineBuilderWrapper(ExperimentalCronetEngine.Builder backend) {
    this.backend = backend;
  }

  @Override
  public String getDefaultUserAgent() {
    return backend.getDefaultUserAgent();
  }

  @Override
  public IHttpEngineBuilder setUserAgent(String userAgent) {
    backend.setUserAgent(userAgent);
    return this;
  }

  @Override
  public IHttpEngineBuilder setStoragePath(String value) {
    backend.setStoragePath(value);
    return this;
  }

  @Override
  public IHttpEngineBuilder setEnableQuic(boolean value) {
    backend.enableQuic(value);
    return this;
  }

  @Override
  public IHttpEngineBuilder setEnableHttp2(boolean value) {
    backend.enableHttp2(value);
    return this;
  }

  @Override
  public IHttpEngineBuilder setEnableBrotli(boolean value) {
    backend.enableBrotli(value);
    return this;
  }

  @Override
  public IHttpEngineBuilder setEnableHttpCache(int cacheMode, long maxSize) {
    backend.enableHttpCache(cacheMode, maxSize);
    return this;
  }

  @Override
  public IHttpEngineBuilder addQuicHint(String host, int port, int alternatePort) {
    backend.addQuicHint(host, port, alternatePort);
    return this;
  }

  @Override
  public IHttpEngineBuilder addPublicKeyPins(
      String hostName, Set<byte[]> pinsSha256, boolean includeSubdomains, Instant expirationInstant) {
    backend.addPublicKeyPins(hostName, pinsSha256, includeSubdomains, Date.from(expirationInstant));
    return this;
  }

  @Override
  public IHttpEngineBuilder setEnablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
    backend.enablePublicKeyPinningBypassForLocalTrustAnchors(value);
    return this;
  }

  @Override
  public IHttpEngineBuilder setQuicOptions(@NonNull QuicOptions options) {
    // If not, we'll have to work around it by modifying the experimental options JSON.
    mExperimentalOptionsPatches.add((experimentalOptions) -> {
      JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");

      // Note: using the experimental APIs always overwrites what's in the experimental
      // JSON, even though "repeated" fields could in theory be additive.
      if (!options.getAllowedQuicHosts().isEmpty()) {
        quicOptions.put("host_whitelist", String.join(",", options.getAllowedQuicHosts()));
      }

      if (options.getHandshakeUserAgent() != null) {
        quicOptions.put("user_agent_id", options.getHandshakeUserAgent());
      }

      if (options.getIdleConnectionTimeout() != null) {
        quicOptions.put("idle_connection_timeout_seconds",
            options.getIdleConnectionTimeout().toSeconds());
      }
    });

    return this;
  }

  private static JSONObject createDefaultIfAbsent(JSONObject jsonObject, String key) {
        JSONObject object = jsonObject.optJSONObject(key);
        if (object == null) {
            object = new JSONObject();
            try {
                jsonObject.put(key, object);
            } catch (JSONException e) {
                throw new IllegalArgumentException(
                        "Failed adding a default object for key [" + key + "]", e);
            }
        }

        return object;
    }

  @Override
  public IHttpEngineBuilder setDnsOptions(@NonNull DnsOptions options) {
        // If not, we'll have to work around it by modifying the experimental options JSON.
        mExperimentalOptionsPatches.add((experimentalOptions) -> {
            JSONObject asyncDnsOptions = createDefaultIfAbsent(experimentalOptions, "AsyncDNS");
            JSONObject staleDnsOptions = createDefaultIfAbsent(experimentalOptions, "StaleDNS");

            if (options.getStaleDns() != DNS_OPTION_UNSPECIFIED) {
                staleDnsOptions.put("enable", options.getStaleDns() == DNS_OPTION_ENABLED);
            }

            if (options.getPersistHostCache() != DNS_OPTION_UNSPECIFIED) {
                staleDnsOptions.put("persist_to_disk", options.getPersistHostCache() == DNS_OPTION_ENABLED);
            }

            if (options.getPersistHostCachePeriod() != null) {
                staleDnsOptions.put("persist_delay_ms", options.getPersistHostCachePeriod().toMillis());
            }

            if (options.getStaleDnsOptions() != null) {
                DnsOptions.StaleDnsOptions staleDnsOptionsJava = options.getStaleDnsOptions();

                if (staleDnsOptionsJava.getAllowCrossNetworkUsage() != DNS_OPTION_UNSPECIFIED) {
                    staleDnsOptions.put(
                            "allow_other_network", staleDnsOptionsJava.getAllowCrossNetworkUsage() == DNS_OPTION_ENABLED);
                }

                if (staleDnsOptionsJava.getFreshLookupTimeout() != null) {
                    staleDnsOptions.put(
                            "delay_ms", staleDnsOptionsJava.getFreshLookupTimeout().toMillis());
                }

                if (staleDnsOptionsJava.getUseStaleOnNameNotResolved() != DNS_OPTION_UNSPECIFIED) {
                    staleDnsOptions.put("use_stale_on_name_not_resolved",
                            staleDnsOptionsJava.getUseStaleOnNameNotResolved() == DNS_OPTION_ENABLED);
                }

                if (staleDnsOptionsJava.getMaxExpiredDelay() != null) {
                    staleDnsOptions.put(
                            "max_expired_time_ms", staleDnsOptionsJava.getMaxExpiredDelay().toMillis());
                }
            }

            JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");
            if (options.getPreestablishConnectionsToStaleDnsResults() != DNS_OPTION_UNSPECIFIED) {
                quicOptions.put("race_stale_dns_on_connection",
                        options.getPreestablishConnectionsToStaleDnsResults() == DNS_OPTION_ENABLED);
      }
    });
    return this;
  }

  @Override
  public IHttpEngineBuilder setConnectionMigrationOptions(@NonNull ConnectionMigrationOptions options) {
        // If not, we'll have to work around it by modifying the experimental options JSON.
        mExperimentalOptionsPatches.add((experimentalOptions) -> {
            JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");

            if (options.getDefaultNetworkMigration() != MIGRATION_OPTION_UNSPECIFIED) {
                quicOptions.put("migrate_sessions_on_network_change_v2",
                        options.getDefaultNetworkMigration() == MIGRATION_OPTION_ENABLED);
            }
            if (options.getPathDegradationMigration() != MIGRATION_OPTION_UNSPECIFIED) {
                boolean pathDegradationValue = options.getPathDegradationMigration() == MIGRATION_OPTION_ENABLED;

                boolean skipPortMigrationFlag = false;

                if (options.getAllowNonDefaultNetworkUsage() != MIGRATION_OPTION_UNSPECIFIED) {
                    boolean nonDefaultNetworkValue = options.getAllowNonDefaultNetworkUsage() == MIGRATION_OPTION_ENABLED;
                    if (!pathDegradationValue && nonDefaultNetworkValue) {
                        // Misconfiguration which doesn't translate easily to the JSON flags
                        throw new IllegalArgumentException(
                                "Unable to turn on non-default network usage without path "
                                + "degradation migration!");
                    } else if (pathDegradationValue && nonDefaultNetworkValue) {
                        // Both values being true results in the non-default network migration
                        // being enabled.
                        quicOptions.put("migrate_sessions_early_v2", true);
                        skipPortMigrationFlag = true;
                    } else {
                        quicOptions.put("migrate_sessions_early_v2", false);
                    }
                }

                if (!skipPortMigrationFlag) {
                    quicOptions.put("allow_port_migration", pathDegradationValue);
                }
            }
        });

        return this;
    }

  @FunctionalInterface
  private interface ExperimentalOptionsPatch {
    void applyTo(JSONObject experimentalOptions) throws JSONException;
  }

  /**
   * Build a {@link CronetEngine} using this builder's configuration.
   *
   * @return constructed {@link CronetEngine}.
   */
  public HttpEngine build() {
    for (ExperimentalOptionsPatch patch : mExperimentalOptionsPatches) {
      try {
        patch.applyTo(mParsedExperimentalOptions);
      } catch (JSONException e) {
        throw new IllegalStateException("Unable to apply JSON patch!", e);
      }
    }
    backend.setExperimentalOptions(mParsedExperimentalOptions.toString());
    return new HttpEngineWrapper(backend.build());
  }
}
