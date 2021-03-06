/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "AndroidTextInputShadowNode.h"

#include <fb/fbjni.h>
#include <react/attributedstring/AttributedStringBox.h>
#include <react/attributedstring/TextAttributes.h>
#include <react/components/text/BaseTextShadowNode.h>
#include <react/core/LayoutConstraints.h>
#include <react/core/LayoutContext.h>
#include <react/core/conversions.h>
#include <react/jni/ReadableNativeMap.h>

using namespace facebook::jni;

namespace facebook {
namespace react {

extern const char AndroidTextInputComponentName[] = "AndroidTextInput";

void AndroidTextInputShadowNode::setContextContainer(
    ContextContainer *contextContainer) {
  ensureUnsealed();
  contextContainer_ = contextContainer;
}

AttributedString AndroidTextInputShadowNode::getAttributedString() const {
  // Use BaseTextShadowNode to get attributed string from children
  auto childTextAttributes = TextAttributes::defaultTextAttributes();
  childTextAttributes.apply(getProps()->textAttributes);
  auto attributedString =
      BaseTextShadowNode::getAttributedString(childTextAttributes, *this);

  // BaseTextShadowNode only gets children. We must detect and prepend text
  // value attributes manually.
  if (!getProps()->text.empty()) {
    auto textAttributes = TextAttributes::defaultTextAttributes();
    textAttributes.apply(getProps()->textAttributes);
    auto fragment = AttributedString::Fragment{};
    fragment.string = getProps()->text;
    fragment.textAttributes = textAttributes;
    // If the TextInput opacity is 0 < n < 1, the opacity of the TextInput and
    // text value's background will stack. This is a hack/workaround to prevent
    // that effect.
    fragment.textAttributes.backgroundColor = clearColor();
    fragment.parentShadowView = ShadowView(*this);
    attributedString.prependFragment(fragment);

    // We know this is not empty, because we at least have the `text` value
    return attributedString;
  }

  // No need to use placeholder if we have text at this point.
  if (!attributedString.isEmpty()) {
    return attributedString;
  }

  return getPlaceholderAttributedString(false);
}

// For measurement purposes, we want to make sure that there's at least a
// single character in the string so that the measured height is greater
// than zero. Otherwise, empty TextInputs with no placeholder don't
// display at all.
AttributedString AndroidTextInputShadowNode::getPlaceholderAttributedString(
    bool ensureMinimumLength) const {
  // Return placeholder text, since text and children are empty.
  auto textAttributedString = AttributedString{};
  auto fragment = AttributedString::Fragment{};
  fragment.string = getProps()->placeholder;

  if (fragment.string.empty() && ensureMinimumLength) {
    fragment.string = " ";
  }

  auto textAttributes = TextAttributes::defaultTextAttributes();
  textAttributes.apply(getProps()->textAttributes);

  // If there's no text, it's possible that this Fragment isn't actually
  // appended to the AttributedString (see implementation of appendFragment)
  fragment.textAttributes = textAttributes;
  fragment.parentShadowView = ShadowView(*this);
  textAttributedString.appendFragment(fragment);

  return textAttributedString;
}

void AndroidTextInputShadowNode::setTextLayoutManager(
    SharedTextLayoutManager textLayoutManager) {
  ensureUnsealed();
  textLayoutManager_ = textLayoutManager;
}

void AndroidTextInputShadowNode::updateStateIfNeeded() {
  ensureUnsealed();

  auto reactTreeAttributedString = getAttributedString();
  auto const &state = getStateData();

  assert(textLayoutManager_);
  assert(
      (!state.layoutManager || state.layoutManager == textLayoutManager_) &&
      "`StateData` refers to a different `TextLayoutManager`");

  // Tree is often out of sync with the value of the TextInput.
  // This is by design - don't change the value of the TextInput in the State,
  // and therefore in Java, unless the tree itself changes.
  if (state.reactTreeAttributedString == reactTreeAttributedString &&
      state.layoutManager == textLayoutManager_) {
    return;
  }

  // Store default TextAttributes in state.
  // In the case where the TextInput is completely empty (no value, no
  // defaultValue, no placeholder, no children) there are therefore no fragments
  // in the AttributedString, and when State is updated, it needs some way to
  // reconstruct a Fragment with default TextAttributes.
  auto defaultTextAttributes = TextAttributes::defaultTextAttributes();
  defaultTextAttributes.apply(getProps()->textAttributes);

  setStateData(AndroidTextInputState{state.mostRecentEventCount,
                                     reactTreeAttributedString,
                                     reactTreeAttributedString,
                                     getProps()->paragraphAttributes,
                                     defaultTextAttributes,
                                     ShadowView(*this),
                                     textLayoutManager_});
}

#pragma mark - LayoutableShadowNode

Size AndroidTextInputShadowNode::measure(
    LayoutConstraints layoutConstraints) const {
  auto const &state = getStateData();

  AttributedString attributedString = state.attributedString;

  if (attributedString.isEmpty()) {
    attributedString = getPlaceholderAttributedString(true);
  }

  if (attributedString.isEmpty()) {
    return {0, 0};
  }

  return textLayoutManager_->measure(
      AttributedStringBox{attributedString},
      getProps()->paragraphAttributes,
      layoutConstraints);
}

void AndroidTextInputShadowNode::layout(LayoutContext layoutContext) {
  updateStateIfNeeded();
  ConcreteViewShadowNode::layout(layoutContext);
}

} // namespace react
} // namespace facebook
