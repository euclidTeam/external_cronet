#ifndef QUICHE_OBLIVIOUS_HTTP_COMMON_OBLIVIOUS_HTTP_HEADER_KEY_CONFIG_H_
#define QUICHE_OBLIVIOUS_HTTP_COMMON_OBLIVIOUS_HTTP_HEADER_KEY_CONFIG_H_

#include <stdint.h>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "openssl/base.h"
#include "quiche/common/platform/api/quiche_export.h"
#include "quiche/common/quiche_data_reader.h"

namespace quiche {

class QUICHE_EXPORT_PRIVATE ObliviousHttpHeaderKeyConfig {
 public:
  // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-03.html#section-4.1-4.2
  static constexpr absl::string_view kOhttpRequestLabel =
      "message/bhttp request";
  static constexpr absl::string_view kOhttpResponseLabel =
      "message/bhttp response";
  // Length of the Oblivious HTTP header.
  static constexpr uint32_t kHeaderLength =
      sizeof(uint8_t) + (3 * sizeof(uint16_t));
  static constexpr absl::string_view kKeyHkdfInfo = "key";
  static constexpr absl::string_view kNonceHkdfInfo = "nonce";

  static absl::StatusOr<ObliviousHttpHeaderKeyConfig> Create(uint8_t key_id,
                                                             uint16_t kem_id,
                                                             uint16_t kdf_id,
                                                             uint16_t aead_id);

  // Copyable to support stack allocated pass-by-value for trivial data members.
  ObliviousHttpHeaderKeyConfig(const ObliviousHttpHeaderKeyConfig& other) =
      default;
  ObliviousHttpHeaderKeyConfig& operator=(
      const ObliviousHttpHeaderKeyConfig& other) = default;

  // Movable.
  ObliviousHttpHeaderKeyConfig(ObliviousHttpHeaderKeyConfig&& other) = default;
  ObliviousHttpHeaderKeyConfig& operator=(
      ObliviousHttpHeaderKeyConfig&& other) = default;

  ~ObliviousHttpHeaderKeyConfig() = default;

  const EVP_HPKE_KEM* GetHpkeKem() const;
  const EVP_HPKE_KDF* GetHpkeKdf() const;
  const EVP_HPKE_AEAD* GetHpkeAead() const;

  uint8_t GetKeyId() const { return key_id_; }
  uint16_t GetHpkeKemId() const { return kem_id_; }
  uint16_t GetHpkeKdfId() const { return kdf_id_; }
  uint16_t GetHpkeAeadId() const { return aead_id_; }

  // Build HPKE context info ["message/bhttp request", 0x00, keyID(1 byte),
  // kemID(2 bytes), kdfID(2 bytes), aeadID(2 bytes)] in network byte order and
  // return a sequence of bytes(bytestring).
  // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-03.html#section-4.1-10
  std::string SerializeRecipientContextInfo() const;

  // TODO(anov): Replace with ObliviousRequest/Response abstraction
  // Parses the below Header
  // [keyID(1 byte), kemID(2 bytes), kdfID(2 bytes), aeadID(2 bytes)]
  // from the payload received in Ohttp Request, and verifies that these values
  // match with the info stored in `this` namely [key_id_, kem_id_, kdf_id_,
  // aead_id_]
  // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-03.html#section-4.1-7
  absl::Status ParseOhttpPayloadHeader(absl::string_view payload_bytes) const;

  // Extracts Key ID from the OHTTP Request payload.
  static absl::StatusOr<uint8_t> ParseKeyIdFromObliviousHttpRequestPayload(
      absl::string_view payload_bytes);

  // Build Request header according to network byte order and return string.
  std::string SerializeOhttpPayloadHeader() const;

 private:
  // Constructor
  explicit ObliviousHttpHeaderKeyConfig(uint8_t key_id, uint16_t kem_id,
                                        uint16_t kdf_id, uint16_t aead_id);

  // Helps validate Key configuration for supported schemes.
  absl::Status ValidateKeyConfig() const;

  // Public Key configuration hosted by Gateway to facilitate Oblivious HTTP
  // HPKE encryption.
  // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-03.html#name-key-configuration-encoding
  uint8_t key_id_;
  uint16_t kem_id_;
  uint16_t kdf_id_;
  uint16_t aead_id_;
};

// Contains multiple ObliviousHttpHeaderKeyConfig objects and associated private
// keys.  An ObliviousHttpHeaderKeyConfigs object can be constructed from the
// "Key Configuration" defined in the Oblivious HTTP spec.  Multiple key
// configurations maybe be supported by the server.
//
// See https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-04.html#section-3
// for details of the "Key Configuration" spec.
//
// ObliviousHttpKeyConfigs objects are immutable after construction.
class QUICHE_EXPORT_PRIVATE ObliviousHttpKeyConfigs {
 public:
  // Parses the "application/ohttp-keys" media type, which is a byte string
  // formatted according to the spec:
  // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-04.html#section-3
  static absl::StatusOr<ObliviousHttpKeyConfigs> ParseConcatenatedKeys(
      absl::string_view key_configs);

  int NumKeys() const { return public_keys_.size(); }

  // Returns a preferred config to use.  The preferred key is the key with
  // the highest key_id.  If more than one configuration exists for the
  // preferred key any configuration may be returned.
  //
  // These methods are useful in the (common) case where only one key
  // configuration is supported by the server.
  ObliviousHttpHeaderKeyConfig PreferredConfig() const;

  absl::StatusOr<absl::string_view> GetPublicKeyForId(uint8_t key_id) const;

  // TODO(kmg): Add methods to somehow access other non-preferred key
  // configurations.

 private:
  using PublicKeyMap = absl::flat_hash_map<uint8_t, std::string>;
  using ConfigMap =
      absl::btree_map<uint8_t, std::vector<ObliviousHttpHeaderKeyConfig>,
                      std::greater<uint8_t>>;

  ObliviousHttpKeyConfigs(ConfigMap cm, PublicKeyMap km)
      : configs_(std::move(cm)), public_keys_(std::move(km)) {}

  static absl::Status ReadSingleKeyConfig(QuicheDataReader& reader,
                                          ConfigMap& configs,
                                          PublicKeyMap& keys);

  // A mapping from key_id to ObliviousHttpHeaderKeyConfig objects for that key.
  const ConfigMap configs_;

  // A mapping from key_id to the public key for that key_id.
  const PublicKeyMap public_keys_;
};

}  // namespace quiche

#endif  // QUICHE_OBLIVIOUS_HTTP_COMMON_OBLIVIOUS_HTTP_HEADER_KEY_CONFIG_H_
