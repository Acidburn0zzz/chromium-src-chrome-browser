// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/fake_invalidation_service.h"

#include "chrome/browser/invalidation/invalidation_service_util.h"
#include "sync/notifier/object_id_invalidation_map.h"

namespace invalidation {

FakeInvalidationService::FakeInvalidationService()
    : client_id_(GenerateInvalidatorClientId()) {
  invalidator_registrar_.UpdateInvalidatorState(syncer::INVALIDATIONS_ENABLED);
}

FakeInvalidationService::~FakeInvalidationService() {
}

// static
KeyedService* FakeInvalidationService::Build(content::BrowserContext* context) {
  return new FakeInvalidationService();
}

void FakeInvalidationService::RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.RegisterHandler(handler);
}

void FakeInvalidationService::UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) {
  invalidator_registrar_.UpdateRegisteredIds(handler, ids);
}

void FakeInvalidationService::UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.UnregisterHandler(handler);
}

syncer::InvalidatorState FakeInvalidationService::GetInvalidatorState() const {
  return invalidator_registrar_.GetInvalidatorState();
}

std::string FakeInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

InvalidationLogger* FakeInvalidationService::GetInvalidationLogger() {
  return NULL;
}

void FakeInvalidationService::SetInvalidatorState(
    syncer::InvalidatorState state) {
  invalidator_registrar_.UpdateInvalidatorState(state);
}

void FakeInvalidationService::EmitInvalidationForTest(
    const syncer::Invalidation& invalidation) {
  // This function might need to modify the invalidator, so we start by making
  // an identical copy of it.
  syncer::Invalidation invalidation_copy(invalidation);

  // If no one is listening to this invalidation, do not send it out.
  syncer::ObjectIdSet registered_ids =
      invalidator_registrar_.GetAllRegisteredIds();
  if (registered_ids.find(invalidation.object_id()) == registered_ids.end()) {
    mock_ack_handler_.RegisterUnsentInvalidation(&invalidation_copy);
    return;
  }

  // Otherwise, register the invalidation with the mock_ack_handler_ and deliver
  // it to the appropriate consumer.
  mock_ack_handler_.RegisterInvalidation(&invalidation_copy);
  syncer::ObjectIdInvalidationMap invalidation_map;
  invalidation_map.Insert(invalidation_copy);
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

syncer::MockAckHandler* FakeInvalidationService::GetMockAckHandler() {
  return &mock_ack_handler_;
}

}  // namespace invalidation
