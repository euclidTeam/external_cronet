// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/common_cert_errors.h"

namespace net::cert_errors {

DEFINE_CERT_ERROR_ID(kInternalError, "Internal error");
DEFINE_CERT_ERROR_ID(kValidityFailedNotAfter, "Time is after notAfter");
DEFINE_CERT_ERROR_ID(kValidityFailedNotBefore, "Time is before notBefore");
DEFINE_CERT_ERROR_ID(kDistrustedByTrustStore, "Distrusted by trust store");

DEFINE_CERT_ERROR_ID(
    kSignatureAlgorithmMismatch,
    "Certificate.signatureAlgorithm != TBSCertificate.signature");

DEFINE_CERT_ERROR_ID(kChainIsEmpty, "Chain is empty");
DEFINE_CERT_ERROR_ID(kChainIsLength1, "Cannot verify a chain of length 1");
DEFINE_CERT_ERROR_ID(kUnconsumedCriticalExtension,
                     "Unconsumed critical extension");
DEFINE_CERT_ERROR_ID(
    kTargetCertInconsistentCaBits,
    "Target certificate looks like a CA but does not set all CA properties");
DEFINE_CERT_ERROR_ID(kKeyCertSignBitNotSet, "keyCertSign bit is not set");
DEFINE_CERT_ERROR_ID(kMaxPathLengthViolated, "max_path_length reached");
DEFINE_CERT_ERROR_ID(kBasicConstraintsIndicatesNotCa,
                     "Basic Constraints indicates not a CA");
DEFINE_CERT_ERROR_ID(kMissingBasicConstraints,
                     "Does not have Basic Constraints");
DEFINE_CERT_ERROR_ID(kNotPermittedByNameConstraints,
                     "Not permitted by name constraints");
DEFINE_CERT_ERROR_ID(kTooManyNameConstraintChecks,
                     "Too many name constraints checks");
DEFINE_CERT_ERROR_ID(kSubjectDoesNotMatchIssuer,
                     "subject does not match issuer");
DEFINE_CERT_ERROR_ID(kVerifySignedDataFailed, "VerifySignedData failed");
DEFINE_CERT_ERROR_ID(kSignatureAlgorithmsDifferentEncoding,
                     "Certificate.signatureAlgorithm is encoded differently "
                     "than TBSCertificate.signature");
DEFINE_CERT_ERROR_ID(kEkuLacksServerAuth,
                     "The extended key usage does not include server auth");
DEFINE_CERT_ERROR_ID(kEkuLacksServerAuthButHasGatedCrypto,
                     "The extended key usage does not include server auth but "
                     "instead includes Netscape Server Gated Crypto");
DEFINE_CERT_ERROR_ID(kEkuLacksClientAuth,
                     "The extended key usage does not include client auth");
DEFINE_CERT_ERROR_ID(kCertIsNotTrustAnchor,
                     "Certificate is not a trust anchor");
DEFINE_CERT_ERROR_ID(kNoValidPolicy, "No valid policy");
DEFINE_CERT_ERROR_ID(kPolicyMappingAnyPolicy,
                     "PolicyMappings must not map anyPolicy");
DEFINE_CERT_ERROR_ID(kFailedParsingSpki, "Couldn't parse SubjectPublicKeyInfo");
DEFINE_CERT_ERROR_ID(kUnacceptableSignatureAlgorithm,
                     "Unacceptable signature algorithm");
DEFINE_CERT_ERROR_ID(kUnacceptablePublicKey, "Unacceptable public key");
DEFINE_CERT_ERROR_ID(kCertificateRevoked, "Certificate is revoked");
DEFINE_CERT_ERROR_ID(kNoRevocationMechanism,
                     "Certificate lacks a revocation mechanism");
DEFINE_CERT_ERROR_ID(kUnableToCheckRevocation, "Unable to check revocation");
DEFINE_CERT_ERROR_ID(kNoIssuersFound, "No matching issuer found");
DEFINE_CERT_ERROR_ID(kDeadlineExceeded, "Deadline exceeded");
DEFINE_CERT_ERROR_ID(kIterationLimitExceeded, "Iteration limit exceeded");
DEFINE_CERT_ERROR_ID(kDepthLimitExceeded, "Depth limit exceeded");

}  // namespace net::cert_errors
