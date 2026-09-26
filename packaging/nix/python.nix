# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{ python314 }:
let
  python =
    (python314.override {
      self = python;
    }).overrideAttrs
      (prev: {
        patches = prev.patches ++ [
          ../../patches/python/v3.14/01-exec-aware-open-code.patch
          ../../patches/python/v3.14/02-exec-aware-main.patch
        ];
        # The standard library is code: mark it executable so that it can
        # still be imported when SECBIT_EXEC_RESTRICT_FILE is set
        postFixup = prev.postFixup + ''
          find "$out/lib" -type f \( -name '*.py' -o -name '*.pyc' \) \
            -exec chmod a+x {} +
        '';
      });
in
python
