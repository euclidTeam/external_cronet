// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/blind_sign_auth/blind_sign_auth.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "anonymous_tokens/cpp/crypto/crypto_utils.h"
#include "anonymous_tokens/cpp/privacy_pass/token_encodings.h"
#include "anonymous_tokens/cpp/testing/proto_utils.h"
#include "anonymous_tokens/cpp/testing/utils.h"
#include "openssl/base.h"
#include "openssl/digest.h"
#include "quiche/blind_sign_auth/blind_sign_auth_protos.h"
#include "quiche/blind_sign_auth/blind_sign_http_interface.h"
#include "quiche/blind_sign_auth/blind_sign_http_response.h"
#include "quiche/blind_sign_auth/test_tools/mock_blind_sign_http_interface.h"
#include "quiche/common/platform/api/quiche_mutex.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche {
namespace test {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StartsWith;
using ::testing::Unused;

class BlindSignAuthTest : public QuicheTest {
 protected:
  void SetUp() override {
    // Create keypair and populate protos.
    auto [test_rsa_public_key, test_rsa_private_key] =
        anonymous_tokens::GetStrongTestRsaKeyPair2048();
    ANON_TOKENS_ASSERT_OK_AND_ASSIGN(
        rsa_public_key_,
        anonymous_tokens::CreatePublicKeyRSA(
            test_rsa_public_key.n, test_rsa_public_key.e));
    ANON_TOKENS_ASSERT_OK_AND_ASSIGN(
        rsa_private_key_,
        anonymous_tokens::CreatePrivateKeyRSA(
            test_rsa_private_key.n, test_rsa_private_key.e,
            test_rsa_private_key.d, test_rsa_private_key.p,
            test_rsa_private_key.q, test_rsa_private_key.dp,
            test_rsa_private_key.dq, test_rsa_private_key.crt));

    anonymous_tokens::RSAPublicKey public_key;
    public_key.set_n(test_rsa_public_key.n);
    public_key.set_e(test_rsa_public_key.e);

    public_key_proto_.set_key_version(1);
    public_key_proto_.set_use_case("TEST_USE_CASE");
    public_key_proto_.set_serialized_public_key(public_key.SerializeAsString());
    public_key_proto_.set_sig_hash_type(
        anonymous_tokens::AT_HASH_TYPE_SHA384);
    public_key_proto_.set_mask_gen_function(
        anonymous_tokens::AT_MGF_SHA384);
    public_key_proto_.set_salt_length(48);
    public_key_proto_.set_key_size(256);
    public_key_proto_.set_message_mask_type(
        anonymous_tokens::AT_MESSAGE_MASK_CONCAT);
    public_key_proto_.set_message_mask_size(32);

    // Create expected GetInitialDataRequest.
    expected_get_initial_data_request_.set_use_attestation(false);
    expected_get_initial_data_request_.set_service_type("chromeipblinding");
    expected_get_initial_data_request_.set_location_granularity(
        privacy::ppn::GetInitialDataRequest_LocationGranularity_CITY_GEOS);

    // Create fake GetInitialDataResponse.
    privacy::ppn::GetInitialDataResponse fake_get_initial_data_response;
    *fake_get_initial_data_response.mutable_at_public_metadata_public_key() =
        public_key_proto_;

    // Create public metadata info.
    privacy::ppn::PublicMetadata::Location location;
    location.set_country("US");
    anonymous_tokens::Timestamp expiration;
    expiration.set_seconds(absl::ToUnixSeconds(absl::Now() + absl::Hours(1)));
    privacy::ppn::PublicMetadata public_metadata;
    *public_metadata.mutable_exit_location() = location;
    public_metadata.set_service_type("chromeipblinding");
    *public_metadata.mutable_expiration() = expiration;
    public_metadata_info_.set_validation_version(1);
    *public_metadata_info_.mutable_public_metadata() = public_metadata;
    *fake_get_initial_data_response.mutable_public_metadata_info() =
        public_metadata_info_;
    fake_get_initial_data_response_ = fake_get_initial_data_response;

    // Create PrivacyPassData.
    privacy::ppn::GetInitialDataResponse::PrivacyPassData privacy_pass_data;
    // token_key_id is derived from public key.
    ANON_TOKENS_ASSERT_OK_AND_ASSIGN(
        std::string public_key_der,
        anonymous_tokens::RsaSsaPssPublicKeyToDerEncoding(
            rsa_public_key_.get()));
    const EVP_MD* sha256 = EVP_sha256();
    ANON_TOKENS_ASSERT_OK_AND_ASSIGN(
        token_key_id_, anonymous_tokens::ComputeHash(
                           public_key_der, *sha256));

    // Create and serialize fake extensions.
    anonymous_tokens::ExpirationTimestamp
        expiration_timestamp;
    int64_t one_hour_away = absl::ToUnixSeconds(absl::Now() + absl::Hours(1));
    expiration_timestamp.timestamp = one_hour_away - (one_hour_away % 900);
    expiration_timestamp.timestamp_precision = 900;
    absl::StatusOr<anonymous_tokens::Extension>
        expiration_extension = expiration_timestamp.AsExtension();
    QUICHE_EXPECT_OK(expiration_extension);
    extensions_.extensions.push_back(*expiration_extension);

    anonymous_tokens::GeoHint geo_hint;
    geo_hint.country_code = "US";
    absl::StatusOr<anonymous_tokens::Extension>
        geo_hint_extension = geo_hint.AsExtension();
    QUICHE_EXPECT_OK(geo_hint_extension);
    extensions_.extensions.push_back(*geo_hint_extension);

    anonymous_tokens::ServiceType service_type;
    service_type.service_type_id =
        anonymous_tokens::ServiceType::kChromeIpBlinding;
    absl::StatusOr<anonymous_tokens::Extension>
        service_type_extension = service_type.AsExtension();
    QUICHE_EXPECT_OK(service_type_extension);
    extensions_.extensions.push_back(*service_type_extension);

    absl::StatusOr<std::string> serialized_extensions =
        anonymous_tokens::EncodeExtensions(extensions_);
    QUICHE_EXPECT_OK(serialized_extensions);

    privacy_pass_data.set_token_key_id(token_key_id_);
    privacy_pass_data.set_public_metadata_extensions(*serialized_extensions);

    *fake_get_initial_data_response.mutable_public_metadata_info() =
        public_metadata_info_;
    *fake_get_initial_data_response.mutable_privacy_pass_data() =
        privacy_pass_data;
    fake_get_initial_data_response_ = fake_get_initial_data_response;

    // Create BlindSignAuthOptions.
    privacy::ppn::BlindSignAuthOptions options;
    options.set_enable_privacy_pass(false);

    blind_sign_auth_ =
        std::make_unique<BlindSignAuth>(&mock_http_interface_, options);
  }

  void TearDown() override {
    blind_sign_auth_.reset(nullptr);
  }

 public:
  void CreateSignResponse(const std::string& body, bool use_privacy_pass) {
    privacy::ppn::AuthAndSignRequest request;
    ASSERT_TRUE(request.ParseFromString(body));

    // Validate AuthAndSignRequest.
    EXPECT_EQ(request.oauth_token(), oauth_token_);
    EXPECT_EQ(request.service_type(), "chromeipblinding");
    // Phosphor does not need the public key hash if the KeyType is
    // privacy::ppn::AT_PUBLIC_METADATA_KEY_TYPE.
    EXPECT_EQ(request.key_type(), privacy::ppn::AT_PUBLIC_METADATA_KEY_TYPE);
    EXPECT_EQ(request.public_key_hash(), "");
    EXPECT_EQ(request.key_version(), public_key_proto_.key_version());
    EXPECT_EQ(request.do_not_use_rsa_public_exponent(), true);
    EXPECT_NE(request.blinded_token().size(), 0);

    if (use_privacy_pass) {
      EXPECT_EQ(request.public_metadata_extensions(),
                fake_get_initial_data_response_.privacy_pass_data()
                    .public_metadata_extensions());
    } else {
      EXPECT_EQ(request.public_metadata_info().SerializeAsString(),
                public_metadata_info_.SerializeAsString());
    }

    // Construct AuthAndSignResponse.
    privacy::ppn::AuthAndSignResponse response;
    for (const auto& request_token : request.blinded_token()) {
      std::string decoded_blinded_token;
      ASSERT_TRUE(absl::Base64Unescape(request_token, &decoded_blinded_token));
      if (use_privacy_pass) {
        absl::StatusOr<std::string> signature =
            anonymous_tokens::TestSignWithPublicMetadata(
                decoded_blinded_token, request.public_metadata_extensions(),
                *rsa_private_key_, false);
        QUICHE_EXPECT_OK(signature);
        response.add_blinded_token_signature(absl::Base64Escape(*signature));
      } else {
        absl::StatusOr<std::string> serialized_token =
            anonymous_tokens::TestSign(
                decoded_blinded_token, rsa_private_key_.get());
        // TestSignWithPublicMetadata for privacy pass
        QUICHE_EXPECT_OK(serialized_token);
        response.add_blinded_token_signature(
            absl::Base64Escape(*serialized_token));
      }
    }
    sign_response_ = response;
  }

  void ValidateGetTokensOutput(absl::Span<BlindSignToken> tokens) {
    for (const auto& token : tokens) {
      privacy::ppn::SpendTokenData spend_token_data;
      ASSERT_TRUE(spend_token_data.ParseFromString(token.token));
      // Validate token structure.
      EXPECT_EQ(spend_token_data.public_metadata().SerializeAsString(),
                public_metadata_info_.public_metadata().SerializeAsString());
      EXPECT_THAT(spend_token_data.unblinded_token(), StartsWith("blind:"));
      EXPECT_GE(spend_token_data.unblinded_token_signature().size(),
                spend_token_data.unblinded_token().size());
      EXPECT_EQ(spend_token_data.signing_key_version(),
                public_key_proto_.key_version());
      EXPECT_NE(spend_token_data.use_case(),
                anonymous_tokens::AnonymousTokensUseCase::
                    ANONYMOUS_TOKENS_USE_CASE_UNDEFINED);
      EXPECT_NE(spend_token_data.message_mask(), "");
    }
  }

  void ValidatePrivacyPassTokensOutput(absl::Span<BlindSignToken> tokens) {
    for (const auto& token : tokens) {
      privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
      ASSERT_TRUE(privacy_pass_token_data.ParseFromString(token.token));
      // Validate token structure.
      std::string decoded_token;
      ASSERT_TRUE(absl::Base64Unescape(privacy_pass_token_data.token(),
                                       &decoded_token));
      std::string decoded_extensions;
      ASSERT_TRUE(absl::Base64Unescape(
          privacy_pass_token_data.encoded_extensions(), &decoded_extensions));
    }
  }

  MockBlindSignHttpInterface mock_http_interface_;
  std::unique_ptr<BlindSignAuth> blind_sign_auth_;
  anonymous_tokens::RSABlindSignaturePublicKey
      public_key_proto_;
  bssl::UniquePtr<RSA> rsa_public_key_;
  bssl::UniquePtr<RSA> rsa_private_key_;
  std::string token_key_id_;
  anonymous_tokens::Extensions extensions_;
  privacy::ppn::PublicMetadataInfo public_metadata_info_;
  privacy::ppn::AuthAndSignResponse sign_response_;
  privacy::ppn::GetInitialDataResponse fake_get_initial_data_response_;
  std::string oauth_token_ = "oauth_token";
  privacy::ppn::GetInitialDataRequest expected_get_initial_data_request_;
};

TEST_F(BlindSignAuthTest, TestGetTokensSuccessful) {
  BlindSignHttpResponse fake_public_key_response(
      200, fake_get_initial_data_response_.SerializeAsString());

  {
    InSequence seq;

    EXPECT_CALL(
        mock_http_interface_,
        DoRequest(
            Eq(BlindSignHttpRequestType::kGetInitialData), Eq(oauth_token_),
            Eq(expected_get_initial_data_request_.SerializeAsString()), _))
        .Times(1)
        .WillOnce([=](auto&&, auto&&, auto&&, auto get_initial_data_cb) {
          std::move(get_initial_data_cb)(fake_public_key_response);
        });

    EXPECT_CALL(mock_http_interface_,
                DoRequest(Eq(BlindSignHttpRequestType::kAuthAndSign),
                          Eq(oauth_token_), _, _))
        .Times(1)
        .WillOnce(Invoke([this](Unused, Unused, const std::string& body,
                                BlindSignHttpCallback callback) {
          CreateSignResponse(body, false);
          BlindSignHttpResponse http_response(
              200, sign_response_.SerializeAsString());
          std::move(callback)(http_response);
        }));
  }

  int num_tokens = 1;
  QuicheNotification done;
  SignedTokenCallback callback =
      [this, &done,
       num_tokens](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(tokens->size(), num_tokens);
        ValidateGetTokensOutput(*tokens);
        done.Notify();
      };
  blind_sign_auth_->GetTokens(oauth_token_, num_tokens, std::move(callback));
  done.WaitForNotification();
}

TEST_F(BlindSignAuthTest, TestGetTokensFailedNetworkError) {
  EXPECT_CALL(mock_http_interface_,
              DoRequest(Eq(BlindSignHttpRequestType::kGetInitialData),
                        Eq(oauth_token_), _, _))
      .Times(1)
      .WillOnce([=](auto&&, auto&&, auto&&, auto get_initial_data_cb) {
        std::move(get_initial_data_cb)(
            absl::InternalError("Failed to create socket"));
      });

  EXPECT_CALL(mock_http_interface_,
              DoRequest(Eq(BlindSignHttpRequestType::kAuthAndSign), _, _, _))
      .Times(0);

  int num_tokens = 1;
  QuicheNotification done;
  SignedTokenCallback callback =
      [&done](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(), absl::StatusCode::kInternal);
        done.Notify();
      };
  blind_sign_auth_->GetTokens(oauth_token_, num_tokens, std::move(callback));
  done.WaitForNotification();
}

TEST_F(BlindSignAuthTest, TestGetTokensFailedBadGetInitialDataResponse) {
  *fake_get_initial_data_response_.mutable_at_public_metadata_public_key()
       ->mutable_use_case() = "SPAM";

  BlindSignHttpResponse fake_public_key_response(
      200, fake_get_initial_data_response_.SerializeAsString());

  EXPECT_CALL(
      mock_http_interface_,
      DoRequest(Eq(BlindSignHttpRequestType::kGetInitialData), Eq(oauth_token_),
                Eq(expected_get_initial_data_request_.SerializeAsString()), _))
      .Times(1)
      .WillOnce([=](auto&&, auto&&, auto&&, auto get_initial_data_cb) {
        std::move(get_initial_data_cb)(fake_public_key_response);
      });

  EXPECT_CALL(mock_http_interface_,
              DoRequest(Eq(BlindSignHttpRequestType::kAuthAndSign), _, _, _))
      .Times(0);

  int num_tokens = 1;
  QuicheNotification done;
  SignedTokenCallback callback =
      [&done](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(), absl::StatusCode::kInvalidArgument);
        done.Notify();
      };
  blind_sign_auth_->GetTokens(oauth_token_, num_tokens, std::move(callback));
  done.WaitForNotification();
}

TEST_F(BlindSignAuthTest, TestGetTokensFailedBadAuthAndSignResponse) {
  BlindSignHttpResponse fake_public_key_response(
      200, fake_get_initial_data_response_.SerializeAsString());
  {
    InSequence seq;

    EXPECT_CALL(
        mock_http_interface_,
        DoRequest(
            Eq(BlindSignHttpRequestType::kGetInitialData), Eq(oauth_token_),
            Eq(expected_get_initial_data_request_.SerializeAsString()), _))
        .Times(1)
        .WillOnce([=](auto&&, auto&&, auto&&, auto get_initial_data_cb) {
          std::move(get_initial_data_cb)(fake_public_key_response);
        });

    EXPECT_CALL(mock_http_interface_,
                DoRequest(Eq(BlindSignHttpRequestType::kAuthAndSign),
                          Eq(oauth_token_), _, _))
        .Times(1)
        .WillOnce(Invoke([this](Unused, Unused, const std::string& body,
                                BlindSignHttpCallback callback) {
          CreateSignResponse(body, false);
          // Add an invalid signature that can't be Base64 decoded.
          sign_response_.add_blinded_token_signature("invalid_signature%");
          BlindSignHttpResponse http_response(
              200, sign_response_.SerializeAsString());
          std::move(callback)(http_response);
        }));
  }

  int num_tokens = 1;
  QuicheNotification done;
  SignedTokenCallback callback =
      [&done](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(), absl::StatusCode::kInternal);
        done.Notify();
      };
  blind_sign_auth_->GetTokens(oauth_token_, num_tokens, std::move(callback));
  done.WaitForNotification();
}

TEST_F(BlindSignAuthTest, TestPrivacyPassGetTokensSucceeds) {
  privacy::ppn::BlindSignAuthOptions options;
  options.set_enable_privacy_pass(true);
  blind_sign_auth_ =
      std::make_unique<BlindSignAuth>(&mock_http_interface_, options);

  public_key_proto_.set_message_mask_type(
      anonymous_tokens::AT_MESSAGE_MASK_NO_MASK);
  public_key_proto_.set_message_mask_size(0);
  *fake_get_initial_data_response_.mutable_at_public_metadata_public_key() =
      public_key_proto_;
  BlindSignHttpResponse fake_public_key_response(
      200, fake_get_initial_data_response_.SerializeAsString());
  {
    InSequence seq;

    EXPECT_CALL(
        mock_http_interface_,
        DoRequest(
            Eq(BlindSignHttpRequestType::kGetInitialData), Eq(oauth_token_),
            Eq(expected_get_initial_data_request_.SerializeAsString()), _))
        .Times(1)
        .WillOnce([=](auto&&, auto&&, auto&&, auto get_initial_data_cb) {
          std::move(get_initial_data_cb)(fake_public_key_response);
        });

    EXPECT_CALL(mock_http_interface_,
                DoRequest(Eq(BlindSignHttpRequestType::kAuthAndSign),
                          Eq(oauth_token_), _, _))
        .Times(1)
        .WillOnce(Invoke([this](Unused, Unused, const std::string& body,
                                BlindSignHttpCallback callback) {
          CreateSignResponse(body, /*use_privacy_pass=*/true);
          BlindSignHttpResponse http_response(
              200, sign_response_.SerializeAsString());
          std::move(callback)(http_response);
        }));
  }

  int num_tokens = 1;
  QuicheNotification done;
  SignedTokenCallback callback =
      [this, &done](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        ValidatePrivacyPassTokensOutput(*tokens);
        done.Notify();
      };
  blind_sign_auth_->GetTokens(oauth_token_, num_tokens, std::move(callback));
  done.WaitForNotification();
}

TEST_F(BlindSignAuthTest, TestPrivacyPassGetTokensFailsWithBadExtensions) {
  privacy::ppn::BlindSignAuthOptions options;
  options.set_enable_privacy_pass(true);
  blind_sign_auth_ =
      std::make_unique<BlindSignAuth>(&mock_http_interface_, options);

  public_key_proto_.set_message_mask_type(
      anonymous_tokens::AT_MESSAGE_MASK_NO_MASK);
  public_key_proto_.set_message_mask_size(0);
  *fake_get_initial_data_response_.mutable_at_public_metadata_public_key() =
      public_key_proto_;
  fake_get_initial_data_response_.mutable_privacy_pass_data()
      ->set_public_metadata_extensions("spam");
  BlindSignHttpResponse fake_public_key_response(
      200, fake_get_initial_data_response_.SerializeAsString());

  EXPECT_CALL(
      mock_http_interface_,
      DoRequest(Eq(BlindSignHttpRequestType::kGetInitialData), Eq(oauth_token_),
                Eq(expected_get_initial_data_request_.SerializeAsString()), _))
      .Times(1)
      .WillOnce([=](auto&&, auto&&, auto&&, auto get_initial_data_cb) {
        std::move(get_initial_data_cb)(fake_public_key_response);
      });

  int num_tokens = 1;
  QuicheNotification done;
  SignedTokenCallback callback =
      [&done](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(), absl::StatusCode::kInvalidArgument);
        done.Notify();
      };
  blind_sign_auth_->GetTokens(oauth_token_, num_tokens, std::move(callback));
  done.WaitForNotification();
}

}  // namespace
}  // namespace test
}  // namespace quiche
