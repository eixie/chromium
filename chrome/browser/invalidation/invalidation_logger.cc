// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_logger.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/invalidation/invalidation_logger_observer.h"
#include "sync/notifier/invalidation_handler.h"

namespace invalidation {
class InvalidationLoggerObserver;

InvalidationLogger::InvalidationLogger()
    : last_invalidator_state_(syncer::TRANSIENT_INVALIDATION_ERROR) {}

InvalidationLogger::~InvalidationLogger() {}

void InvalidationLogger::OnRegistration(const std::string& registrar_name) {
  registered_handlers_.insert(registrar_name);
  EmitRegisteredHandlers();
}

void InvalidationLogger::OnUnregistration(const std::string& registrar_name) {
  DCHECK(registered_handlers_.find(registrar_name) !=
         registered_handlers_.end());
  std::multiset<std::string>::iterator it =
      registered_handlers_.find(registrar_name);
  // Delete only one instance of registrar_name.
  registered_handlers_.erase(it);
  EmitRegisteredHandlers();
}

void InvalidationLogger::EmitRegisteredHandlers() {
  FOR_EACH_OBSERVER(InvalidationLoggerObserver, observer_list_,
                    OnRegistrationChange(registered_handlers_));
}

void InvalidationLogger::OnStateChange(
    const syncer::InvalidatorState& newState) {
  last_invalidator_state_ = newState;
  EmitState();
}

void InvalidationLogger::EmitState() {
  FOR_EACH_OBSERVER(InvalidationLoggerObserver,
                    observer_list_,
                    OnStateChange(last_invalidator_state_));
}

void InvalidationLogger::OnUpdateIds(
    std::map<std::string, syncer::ObjectIdSet> updated_ids) {
  for (std::map<std::string, syncer::ObjectIdSet>::const_iterator it =
       updated_ids.begin(); it != updated_ids.end(); ++it) {
    latest_ids_[it->first] = syncer::ObjectIdSet(it->second);
  }
  EmitUpdatedIds();
}

void InvalidationLogger::EmitUpdatedIds() {
  for (std::map<std::string, syncer::ObjectIdSet>::const_iterator it =
       latest_ids_.begin(); it != latest_ids_.end(); ++it) {
    const syncer::ObjectIdSet& object_ids_for_handler = it->second;
    syncer::ObjectIdCountMap per_object_invalidation_count;
    for (syncer::ObjectIdSet::const_iterator oid_it =
             object_ids_for_handler.begin();
         oid_it != object_ids_for_handler.end();
         ++oid_it) {
      per_object_invalidation_count[*oid_it] = invalidation_count_[*oid_it];
    }
    FOR_EACH_OBSERVER(InvalidationLoggerObserver,
                      observer_list_,
                      OnUpdateIds(it->first, per_object_invalidation_count));
  }
}

void InvalidationLogger::OnDebugMessage(const base::DictionaryValue& details) {
  FOR_EACH_OBSERVER(
      InvalidationLoggerObserver, observer_list_, OnDebugMessage(details));
}

void InvalidationLogger::OnInvalidation(
    const syncer::ObjectIdInvalidationMap& details) {
  std::vector<syncer::Invalidation> internal_invalidations;
  details.GetAllInvalidations(&internal_invalidations);
  for (std::vector<syncer::Invalidation>::const_iterator it =
           internal_invalidations.begin();
       it != internal_invalidations.end();
       ++it) {
    invalidation_count_[it->object_id()]++;
  }
  FOR_EACH_OBSERVER(
      InvalidationLoggerObserver, observer_list_, OnInvalidation(details));
}

void InvalidationLogger::EmitContent() {
  EmitState();
  EmitUpdatedIds();
  EmitRegisteredHandlers();
}

void InvalidationLogger::RegisterObserver(
    InvalidationLoggerObserver* debug_observer) {
  observer_list_.AddObserver(debug_observer);
}

void InvalidationLogger::UnregisterObserver(
    InvalidationLoggerObserver* debug_observer) {
  observer_list_.RemoveObserver(debug_observer);
}

bool InvalidationLogger::IsObserverRegistered(
    InvalidationLoggerObserver* debug_observer) {
  return observer_list_.HasObserver(debug_observer);
}
}  // namespace invalidation
