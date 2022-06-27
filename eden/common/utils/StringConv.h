/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <folly/Range.h>
#include <folly/String.h>
#include <folly/portability/Windows.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>

#include "eden/common/utils/WinError.h"

namespace facebook::eden {

#ifdef _WIN32

/**
 * Convert a wide string to a utf-8 encoded string.
 */
template <class MultiByteStringType>
MultiByteStringType wideToMultibyteString(std::wstring_view wideCharPiece) {
  if (wideCharPiece.empty()) {
    return MultiByteStringType{};
  }

  int inputSize = folly::to_narrow(folly::to_signed(wideCharPiece.size()));

  // To avoid extra copy or using max size buffers we should get the size first
  // and allocate the right size buffer.
  int size = WideCharToMultiByte(
      CP_UTF8, 0, wideCharPiece.data(), inputSize, nullptr, 0, 0, 0);

  if (size > 0) {
    MultiByteStringType multiByteString(size, 0);
    int resultSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        wideCharPiece.data(),
        inputSize,
        multiByteString.data(),
        size,
        0,
        0);
    if (size == resultSize) {
      return multiByteString;
    }
  }
  throw makeWin32ErrorExplicit(
      GetLastError(), "Failed to convert wide char to char");
}

/**
 * Convert a utf-8 encoded string to a wide string.
 */
std::wstring multibyteToWideString(folly::StringPiece multiBytePiece);

#endif

} // namespace facebook::eden
