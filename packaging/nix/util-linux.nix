# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

# Reuse the nixpkgs build recipe, updated to 2.42.4
{
  lib,
  fetchurl,
  asciidoctor,
  util-linux,
}:
(util-linux.override { translateManpages = false; }).overrideAttrs (
  finalAttrs: prevAttrs: {
    version = "2.42.4";

    src = fetchurl {
      url = "mirror://kernel/linux/utils/util-linux/v${lib.versions.majorMinor finalAttrs.version}/util-linux-${finalAttrs.version}.tar.xz";
      hash = "sha256-+9YqEAq3u4dGugZhJVw8SBhbHpAhUHxiTaAfvGljMOw=";
    };

    # The other nixpkgs patches are backports that 2.42.4 already includes
    patches =
      lib.filter (
        p: lib.hasSuffix "rtcwake-search-PATH-for-shutdown.patch" (toString p)
      ) prevAttrs.patches
      ++ [
        ../../patches/util-linux/v2.42/01-setpriv-add-new-securebits.patch
        ../../patches/util-linux/v2.42/02-setpriv-improve-securebits-man-page.patch
        ../../patches/util-linux/v2.42/03-exec-aware-setpriv.patch
        ../../patches/util-linux/v2.42/04-exec-aware-enosys.patch
      ];

    # Regenerate the man pages that the patches change
    nativeBuildInputs = prevAttrs.nativeBuildInputs ++ [ asciidoctor ];
  }
)
