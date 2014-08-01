# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1', {
      'includes': {
        '../xwalk/build/common.gypi',
      },
    }],
  ]
}
