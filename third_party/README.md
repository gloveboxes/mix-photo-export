# Vendored Dependencies

## libfpx

- Upstream: https://github.com/ImageMagick/libfpx
- Version: 1.3.1-10
- Commit: `bb28e42245701ef5473ac086e06d505491ad3d8a`
- Source directory: `libfpx/`
- Upstream file modifications: none.
- Local addition: `libfpx/config/.gitignore` ensures the required upstream
   `config.guess` and `config.sub` scripts are tracked despite upstream ignore rules.

The directory contains the complete 438-file Git snapshot at this revision,
exported with `git archive`. All source files, upstream documentation, notices,
and executable permissions are preserved. Git metadata, downloaded packages,
and generated build outputs are not included. This is an ordinary source copy,
not a Git submodule; cloning MIX Photo Export includes the dependency.

### Licensing

Preserve the upstream notices, including `libfpx/flashpix.h`,
`libfpx/oless/flashpix.h`, `libfpx/AUTHORS`, and individual file notices.
The upstream README refers to a COPYING file, but that file is absent from
the pinned upstream tree; do not interpret that reference as an extra license.
See the included notices for the actual terms, including attribution,
identification of modifications, advertising acknowledgement, and trademarks.
The dependency's terms remain separate from those for this application's code.

This product includes software developed by the contributors and Digital
Imaging Group, Inc. (http://www.digitalimaging.org/) for use in the Flashpix
Toolkit Project.

### Build and Maintenance

CMake invokes the vendored `configure` script and Makefiles without downloading
libfpx. The library is compiled as C++98 and statically linked into the C++17
application. Generated headers and library objects stay under `build/libfpx/`,
not in the vendored tree. The container compilation and regression-test step
runs with networking disabled; Alpine and package installation still need
network access unless cached.

To update the dependency:

1. Review upstream changes, security fixes, and all licensing notices.
2. Export the selected upstream commit into a clean replacement for `libfpx/`.
   Do not copy a configured build tree or overlay it onto an older snapshot,
   since upstream deletions would otherwise be missed.
3. Update the revision, version, file count, and modification record above.
   Clearly document any local patches; retain original notices.
4. Use fresh build directories or uncached container builds, run both Docker
   and Apple regression suites, and verify production-image conversion.
5. Review the source diff and commit the snapshot with its provenance changes.

Vendoring protects against upstream source disappearance and unexpected source
changes. It does not automatically provide security updates, freeze toolchains
or OS packages, or guarantee future compiler compatibility.