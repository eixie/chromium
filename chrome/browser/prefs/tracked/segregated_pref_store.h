// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_SEGREGATED_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_TRACKED_SEGREGATED_PREF_STORE_H_

#include <set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/persistent_pref_store.h"

// Provides a unified PersistentPrefStore implementation that splits its storage
// and retrieval between two underlying PersistentPrefStore instances: a set of
// preference names is used to partition the preferences.
//
// Combines properties of the two stores as follows:
//   * The unified read error will be:
//                           Selected Store Error
//    Default Store Error | NO_ERROR      | NO_FILE       | other selected |
//               NO_ERROR | NO_ERROR      | NO_ERROR      | other selected |
//               NO_FILE  | NO_FILE       | NO_FILE       | NO_FILE        |
//          other default | other default | other default | other default  |
//   * The unified initialization success, initialization completion, and
//     read-only state are the boolean OR of the underlying stores' properties.
class SegregatedPrefStore : public PersistentPrefStore {
 public:
  // Creates an instance that delegates to |selected_pref_store| for the
  // preferences named in |selected_pref_names| and to |default_pref_store|
  // for all others. If an unselected preference is present in
  // |selected_pref_store| (i.e. because it was previously selected) it will
  // be migrated back to |default_pref_store| upon access via a non-const
  // method.
  // |on_initialization| will be invoked when both stores have been initialized,
  // before observers of the combined store are notified.
  SegregatedPrefStore(
      const scoped_refptr<PersistentPrefStore>& default_pref_store,
      const scoped_refptr<PersistentPrefStore>& selected_pref_store,
      const std::set<std::string>& selected_pref_names,
      const base::Closure& on_initialization);

  // PrefStore implementation
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObservers() const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const OVERRIDE;

  // WriteablePrefStore implementation
  virtual void SetValue(const std::string& key, base::Value* value) OVERRIDE;
  virtual void RemoveValue(const std::string& key) OVERRIDE;

  // PersistentPrefStore implementation
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) OVERRIDE;
  virtual void ReportValueChanged(const std::string& key) OVERRIDE;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) OVERRIDE;
  virtual bool ReadOnly() const OVERRIDE;
  virtual PrefReadError GetReadError() const OVERRIDE;
  virtual PrefReadError ReadPrefs() OVERRIDE;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) OVERRIDE;
  virtual void CommitPendingWrite() OVERRIDE;

 private:
  // Aggregates events from the underlying stores and synthesizes external
  // events via |on_initialization|, |read_error_delegate_|, and |observers_|.
  class AggregatingObserver : public PrefStore::Observer {
   public:
    explicit AggregatingObserver(SegregatedPrefStore* outer);

    // PrefStore::Observer implementation
    virtual void OnPrefValueChanged(const std::string& key) OVERRIDE;
    virtual void OnInitializationCompleted(bool succeeded) OVERRIDE;

   private:
    SegregatedPrefStore* outer_;
    int failed_sub_initializations_;
    int successful_sub_initializations_;

    DISALLOW_COPY_AND_ASSIGN(AggregatingObserver);
  };

  virtual ~SegregatedPrefStore();

  // Returns |selected_pref_store| if |key| is selected or a value is present
  // in |selected_pref_store|. This could happen if |key| was previously
  // selected.
  const PersistentPrefStore* StoreForKey(const std::string& key) const;

  // Returns |selected_pref_store| if |key| is selected. If |key| is
  // unselected but has a value in |selected_pref_store|, moves the value to
  // |default_pref_store| before returning |default_pref_store|.
  PersistentPrefStore* StoreForKey(const std::string& key);

  scoped_refptr<PersistentPrefStore> default_pref_store_;
  scoped_refptr<PersistentPrefStore> selected_pref_store_;
  std::set<std::string> selected_preference_names_;
  base::Closure on_initialization_;

  scoped_ptr<PersistentPrefStore::ReadErrorDelegate> read_error_delegate_;
  ObserverList<PrefStore::Observer, true> observers_;
  AggregatingObserver aggregating_observer_;

  DISALLOW_COPY_AND_ASSIGN(SegregatedPrefStore);
};

#endif  // CHROME_BROWSER_PREFS_TRACKED_SEGREGATED_PREF_STORE_H_
