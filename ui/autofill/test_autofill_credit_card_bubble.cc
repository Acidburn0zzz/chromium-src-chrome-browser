// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test_autofill_credit_card_bubble.h"

namespace autofill {

TestAutofillCreditCardBubble::TestAutofillCreditCardBubble(
    const base::WeakPtr<AutofillCreditCardBubbleController>& controller)
    : showing_(false),
      weak_ptr_factory_(this) {}

TestAutofillCreditCardBubble::~TestAutofillCreditCardBubble() {}

// AutofillCreditCardBubble:
void TestAutofillCreditCardBubble::Show() {
  showing_ = true;
}

void TestAutofillCreditCardBubble::Hide() {
  showing_ = false;
}

bool TestAutofillCreditCardBubble::IsHiding() const {
  return !showing_;
}

base::WeakPtr<TestAutofillCreditCardBubble>
    TestAutofillCreditCardBubble::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
