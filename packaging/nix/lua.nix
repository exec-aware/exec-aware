# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{ lua5_5 }:
let
  lua =
    (lua5_5.override {
      self = lua;
      version = "5.5.1";
      hash = "sha256-HEtAaNZwYfKiIxrStUIud6zqFIfqmJD2Mgr2FPQ3Pc4=";
    }).overrideAttrs
      (prev: {
        patches = prev.patches ++ [ ../../patches/lua/v5.5/01-exec-aware-lua.patch ];
      });
in
lua
