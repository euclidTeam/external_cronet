// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

// The local test root certificate.
const char kRootCertificateFile[] = "root_ca_cert.pem";
// A certificate issued by the local test root for 127.0.0.1.
const char kGoodCertificateFile[] = "ok_cert.pem";

scoped_refptr<CertVerifyProc> CreateCertVerifyProc() {
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  if (base::FeatureList::IsEnabled(features::kChromeRootStoreUsed)) {
    return CertVerifyProc::CreateBuiltinWithChromeRootStore(
        /*cert_net_fetcher=*/nullptr);
  }
#endif
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return CertVerifyProc::CreateBuiltinVerifyProc(/*cert_net_fetcher=*/nullptr);
#else
  return CertVerifyProc::CreateSystemVerifyProc(/*cert_net_fetcher=*/nullptr);
#endif
}

}  // namespace

// Test basic functionality when adding from an existing X509Certificate.
TEST(TestRootCertsTest, AddFromPointer) {
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kRootCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), root_cert.get());

  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(nullptr), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  {
    ScopedTestRoot scoped_root(root_cert.get());
    EXPECT_FALSE(test_roots->IsEmpty());
  }
  EXPECT_TRUE(test_roots->IsEmpty());
}

// Test that TestRootCerts actually adds the appropriate trust status flags
// when requested, and that the trusted status is cleared once the root is
// removed the TestRootCerts. This test acts as a canary/sanity check for
// the results of the rest of net_unittests, ensuring that the trust status
// is properly being set and cleared.
TEST(TestRootCertsTest, OverrideTrust) {
  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(nullptr), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  scoped_refptr<X509Certificate> test_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kGoodCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  // Test that the good certificate fails verification, because the root
  // certificate should not yet be trusted.
  int flags = 0;
  CertVerifyResult bad_verify_result;
  scoped_refptr<CertVerifyProc> verify_proc(CreateCertVerifyProc());
  int bad_status = verify_proc->Verify(
      test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, net::CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &bad_verify_result, NetLogWithSource());
  EXPECT_NE(OK, bad_status);
  EXPECT_NE(0u, bad_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);

  // Add the root certificate and mark it as trusted.
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kRootCertificateFile);
  ASSERT_TRUE(root_cert);
  ScopedTestRoot scoped_root(root_cert.get());
  EXPECT_FALSE(test_roots->IsEmpty());

  // Test that the certificate verification now succeeds, because the
  // TestRootCerts is successfully imbuing trust.
  CertVerifyResult good_verify_result;
  int good_status = verify_proc->Verify(
      test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &good_verify_result, NetLogWithSource());
  EXPECT_THAT(good_status, IsOk());
  EXPECT_EQ(0u, good_verify_result.cert_status);

  test_roots->Clear();
  EXPECT_TRUE(test_roots->IsEmpty());

  // Ensure that when the TestRootCerts is cleared, the trust settings
  // revert to their original state, and don't linger. If trust status
  // lingers, it will likely break other tests in net_unittests.
  CertVerifyResult restored_verify_result;
  int restored_status = verify_proc->Verify(
      test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &restored_verify_result, NetLogWithSource());
  EXPECT_NE(OK, restored_status);
  EXPECT_NE(0u,
            restored_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(bad_status, restored_status);
  EXPECT_EQ(bad_verify_result.cert_status, restored_verify_result.cert_status);
}

TEST(TestRootCertsTest, Moveable) {
  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(nullptr), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  scoped_refptr<X509Certificate> test_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kGoodCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  int flags = 0;
  CertVerifyResult bad_verify_result;
  int bad_status;
  scoped_refptr<CertVerifyProc> verify_proc(CreateCertVerifyProc());
  {
    // Empty ScopedTestRoot at outer scope has no effect.
    ScopedTestRoot scoped_root_outer;
    EXPECT_TRUE(test_roots->IsEmpty());

    // Test that the good certificate fails verification, because the root
    // certificate should not yet be trusted.
    bad_status = verify_proc->Verify(
        test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), flags, net::CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &bad_verify_result, NetLogWithSource());
    EXPECT_NE(OK, bad_status);
    EXPECT_NE(0u,
              bad_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);

    {
      // Add the root certificate and mark it as trusted.
      scoped_refptr<X509Certificate> root_cert =
          ImportCertFromFile(GetTestCertsDirectory(), kRootCertificateFile);
      ASSERT_TRUE(root_cert);
      ScopedTestRoot scoped_root_inner(root_cert.get());
      EXPECT_FALSE(test_roots->IsEmpty());

      // Test that the certificate verification now succeeds, because the
      // TestRootCerts is successfully imbuing trust.
      CertVerifyResult good_verify_result;
      int good_status = verify_proc->Verify(
          test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
          /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
          CertificateList(), &good_verify_result, NetLogWithSource());
      EXPECT_THAT(good_status, IsOk());
      EXPECT_EQ(0u, good_verify_result.cert_status);

      EXPECT_FALSE(scoped_root_inner.IsEmpty());
      EXPECT_TRUE(scoped_root_outer.IsEmpty());
      // Move from inner scoped root to outer
      scoped_root_outer = std::move(scoped_root_inner);
      EXPECT_FALSE(test_roots->IsEmpty());
      EXPECT_FALSE(scoped_root_outer.IsEmpty());
    }
    // After inner scoper was freed, test root is still trusted since ownership
    // was moved to the outer scoper.
    EXPECT_FALSE(test_roots->IsEmpty());
    EXPECT_FALSE(scoped_root_outer.IsEmpty());

    // Test that the certificate verification still succeeds, because the
    // TestRootCerts is successfully imbuing trust.
    CertVerifyResult good_verify_result;
    int good_status = verify_proc->Verify(
        test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &good_verify_result, NetLogWithSource());
    EXPECT_THAT(good_status, IsOk());
    EXPECT_EQ(0u, good_verify_result.cert_status);
  }
  EXPECT_TRUE(test_roots->IsEmpty());

  // Ensure that when the TestRootCerts is cleared, the trust settings
  // revert to their original state, and don't linger. If trust status
  // lingers, it will likely break other tests in net_unittests.
  CertVerifyResult restored_verify_result;
  int restored_status = verify_proc->Verify(
      test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &restored_verify_result, NetLogWithSource());
  EXPECT_NE(OK, restored_status);
  EXPECT_NE(0u,
            restored_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(bad_status, restored_status);
  EXPECT_EQ(bad_verify_result.cert_status, restored_verify_result.cert_status);
}

// TODO(rsleevi): Add tests for revocation checking via CRLs, ensuring that
// TestRootCerts properly injects itself into the validation process. See
// http://crbug.com/63958

}  // namespace net
