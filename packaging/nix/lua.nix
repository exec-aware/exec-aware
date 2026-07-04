# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{ lua5_5 }:
lua5_5.overrideAttrs (prev: {
  patches = prev.patches ++ [ ../../patches/lua/v5.5.0/01-exec-aware-lua.patch ];
})
