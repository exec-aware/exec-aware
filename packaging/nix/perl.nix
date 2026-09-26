# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

# Reuse the nixpkgs build recipe and its patches, which still apply to 5.44
{
  fetchurl,
  perl,
}:
perl.overrideAttrs (
  finalAttrs: prevAttrs: {
    version = "5.44.0";

    src = fetchurl {
      url = "mirror://cpan/src/5.0/perl-${finalAttrs.version}.tar.gz";
      hash = "sha256-O4VQZrkkkctA6Gr/scpX0aOIqkPlG5HHgGoywvZflsM=";
    };

    patches = prevAttrs.patches ++ [
      ../../patches/perl/v5.44/01-exec-aware-perl.patch
    ];

    # Perl installs its library read-only, but modules are code: mark them
    # executable so they still load under EXEC_RESTRICT_FILE
    postInstall = prevAttrs.postInstall + ''
      find "$out/lib/perl5" -type f \( -name '*.pm' -o -name '*.pl' -o -name '*.al' -o -name '*.ix' \) \
        -exec chmod a+x {} +
    '';
  }
)
