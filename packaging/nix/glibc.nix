# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

# Reuse the nixpkgs build recipe, but drop its patches since they target
# the glibc version packaged by nixpkgs
{
  fetchurl,
  glibc,
}:
glibc.overrideAttrs (
  finalAttrs: prevAttrs: {
    version = "2.44";

    src = fetchurl {
      url = "mirror://gnu/glibc/glibc-${finalAttrs.version}.tar.xz";
      hash = "sha256-N/YA8r7zxegwAUcFlWiyouQKetbMxlzpQlVtSUKcxmc=";
    };

    patches = [
      ../../patches/glibc/v2.44/01-exec-aware-ld-so.patch
    ];
  }
)
