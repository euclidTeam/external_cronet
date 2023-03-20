// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_database.h"

#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "build/build_config.h"
#include "net/log/net_log.h"
#include "net/log/net_log_values.h"

namespace net {

// static
CertDatabase* CertDatabase::GetInstance() {
  // Leaky so it can be initialized on worker threads, and because there is no
  // useful cleanup to do.
  return base::Singleton<CertDatabase,
                         base::LeakySingletonTraits<CertDatabase>>::get();
}

void CertDatabase::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void CertDatabase::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void CertDatabase::NotifyObserversCertDBChanged() {
  // Log to NetLog as it may help debug issues like https://crbug.com/915463
  // This isn't guarded with net::NetLog::Get()->IsCapturing()) because an
  // AddGlobalEntry() call without much computation is really cheap.
  net::NetLog::Get()->AddGlobalEntry(
      NetLogEventType::CERTIFICATE_DATABASE_CHANGED);

  observer_list_->Notify(FROM_HERE, &Observer::OnCertDBChanged);
}

CertDatabase::CertDatabase()
    : observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {}

CertDatabase::~CertDatabase() {
#if BUILDFLAG(IS_MAC)
  ReleaseNotifier();
#endif
}

}  // namespace net
