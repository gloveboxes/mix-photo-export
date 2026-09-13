# MIX Photo Export

Recover photos from Microsoft Picture It! `.mix` documents and export them as
JPEG images at their highest stored resolution. Standalone FlashPix `.fpx`
images are supported too.

MIX Photo Export uses the ImageMagick-maintained [FlashPix toolkit](https://github.com/ImageMagick/libfpx)
for decoding and [libjpeg-turbo](https://libjpeg-turbo.org/) for JPEG encoding.
It decodes the image tiles, including shared JPEG tables, rather than simply
extracting a thumbnail or renaming an extension. No Apple imaging frameworks
are required.

**Pinned library source:** This repository includes an unmodified copy of
libfpx 1.3.1-10 in [third_party/libfpx](third_party/libfpx), taken from
[ImageMagick/libfpx at commit bb28e42245701ef5473ac086e06d505491ad3d8a](https://github.com/ImageMagick/libfpx/tree/bb28e42245701ef5473ac086e06d505491ad3d8a).
The toolkit originated with Digital Imaging Group and Eastman Kodak.
Builds use this included source, not an upstream download, so the legacy
library remains available even if its upstream repository disappears.
See [dependency provenance and licensing](third_party/README.md) for details.

Run it on **Windows, macOS, or Linux using Docker**, or use **Apple containers
on Apple Silicon**. This is a **container-only solution**: compilation, tests,
and conversion run inside containers. No compiler or image libraries need to
be installed on the host. The executable inside the image is `mix-photo-export`;
the project name is MIX Photo Export.

## Contents

- [Features and limitations](#features-and-limitations)
- [Platform support](#platform-support)
- [Docker](#docker)
- [Apple containers](#apple-containers)
- [Command-line reference](#command-line-reference)
- [Testing](#testing)
- [Implementation](#implementation)
- [Troubleshooting](#troubleshooting)
- [Licensing](#licensing)

## Features and Limitations

- Convert a single file or a folder of `.mix` and `.fpx` files.
- Choose input/output folders and JPEG quality.
- Export every recognized top-level image store at its highest stored resolution.
- Flatten premultiplied transparency onto white.
- Protect originals by decoding temporary copies.
- Skip existing output files instead of overwriting them.
- Report ordinary per-image errors and continue the batch.

This recovers **embedded image pixels, not the complete Picture It! document
layout**. It does not reconstruct text, layers, frame positioning, or page
compositions. Extra image stores are exported separately. FlashPix viewing
transforms, including orientation/cropping/color adjustments, are not applied.
Source metadata and ICC profiles are not copied.

Folder scanning is nonrecursive. Other unrelated formats using the `.mix`
extension and arbitrary nested compound-document layouts are not supported.

## Platform Support

| Environment | Build/run route | Verification so far |
| --- | --- | --- |
| macOS, Apple Silicon | Docker or Apple containers | Both container runtimes tested with real MIX files |
| macOS, Intel | Docker | amd64 Linux image tested under emulation; Intel Mac host untested |
| Linux, arm64 | Docker | Alpine/musl build and conversion tested in containers |
| Linux, amd64 | Docker | Alpine image built and tested under Docker emulation |
| Windows | Docker Desktop in Linux-container mode | Same Linux image; Windows host commands not executed here |

All routes use the same Linux container image. Apple containers run that image
in lightweight VMs on Apple Silicon, without Docker Desktop.

## Docker

### Prerequisites

Install and start Docker Desktop on Windows/macOS, or Docker Engine on Linux.
Windows must use **Linux containers**, normally with Docker Desktop's WSL 2
backend. Download or clone this repository and open a terminal at its root.

The first build needs Internet access to download Alpine and packages. The
pinned FlashPix source is included in this repository; compilation, tests, and
conversion need no Internet access after the build dependencies are installed.

### Build the Image

```sh
docker build -t mix-photo-export:local .
docker run --rm --network none mix-photo-export:local --help
```

The shared [Dockerfile](Dockerfile) uses **Alpine 3.23** and multiple stages.
It compiles the app, runs generated conversion tests, and strips the executable.
The final image contains the app and runtime libraries, without CMake, Git,
compilers, or test fixtures. The initial arm64 build reported approximately
5.8 MB through `docker image inspect`; reported size depends on image-store
accounting, architecture, and package updates.

Only explicitly allowed source/test files enter the build context. Local photos,
build artifacts, and exports are excluded by [.dockerignore](.dockerignore).
Photos are not uploaded or baked into the image.

### Convert on macOS or Linux

The Bash launcher resolves host paths, creates the output folder, and mounts
the source folder read-only:

```sh
bash scripts/mix-photo-export --runtime docker convert \
  "/absolute/path/to/mix photos" "/absolute/path/to/jpg photos"
```

Or use the Make targets:

```sh
make docker-build
make docker-convert INPUT="/path/to/mix photos" OUTPUT="/path/to/jpg photos" QUALITY=95
make docker-test
```

The launcher runs as the invoking user's UID/GID to avoid root-owned exports
on Linux. It disables networking and capabilities, makes the root filesystem
read-only, and provides a 1 GiB temporary filesystem. Container memory is
limited to 2 GiB. The runtime image defaults to non-root UID/GID `10001:10001`
when invoked directly.

For a direct Docker invocation:

```sh
mkdir -p /absolute/path/to/jpg
docker run --rm --network none --read-only \
  --tmpfs /tmp:rw,noexec,nosuid,size=1g \
  --cap-drop ALL --security-opt no-new-privileges \
  --user "$(id -u):$(id -g)" \
  --mount "type=bind,source=/absolute/path/to/mix,target=/input,readonly" \
  --mount "type=bind,source=/absolute/path/to/jpg,target=/output" \
  mix-photo-export:local -input /input -output /output --quality 95
```

Keep the input and output folders distinct. Bind mounts use absolute host paths;
the app sees `/input` and `/output`, not the host paths. Commas in host paths are
not supported by the launcher because the mount syntax uses comma separators.

### Convert on Windows with PowerShell

Run these commands from the repository root with Docker Desktop running:

```powershell
docker build -t mix-photo-export:local .

$InputFolder = (Resolve-Path "C:\Photos\MIX").Path
$OutputFolder = "C:\Photos\JPEG"
New-Item -ItemType Directory -Force -Path $OutputFolder | Out-Null
$OutputFolder = (Resolve-Path $OutputFolder).Path

docker run --rm --network none --read-only `
  --tmpfs /tmp:rw,noexec,nosuid,size=1g `
  --cap-drop ALL --security-opt no-new-privileges `
  --mount "type=bind,source=$InputFolder,target=/input,readonly" `
  --mount "type=bind,source=$OutputFolder,target=/output" `
  mix-photo-export:local -input /input -output /output --quality 95
```

Allow Docker Desktop access to the selected folders when prompted. No Bash,
Make, or C++ compiler is required for this Docker workflow on Windows.

### Architectures

Normal builds target the runtime's default Linux architecture. The Dockerfile
has been built and tested for `linux/arm64` and `linux/amd64`. To build a single
architecture explicitly with Docker Buildx:

```sh
docker buildx build --platform linux/amd64 --load -t mix-photo-export:amd64 .
docker run --rm --platform linux/amd64 mix-photo-export:amd64 --help
```

Cross-architecture builds require an emulation-enabled builder and are slower.
No prebuilt public image or registry publishing is configured by default.

## Apple Containers

### Apple Runtime Requirements

Use an Apple Silicon Mac with a macOS version supported by
[Apple's container project](https://github.com/apple/container). Install the
`container` CLI using that project's installation instructions. This workflow
does **not** require Docker Desktop.

The implementation follows the shared-Dockerfile and runtime-selector pattern
used by `pcb-agent`. The Apple build/run path was exercised locally with
`container` CLI 1.4.1 on Apple Silicon.

### Build and Convert

```sh
container system start
make apple-build
make apple-convert INPUT="/path/to/mix photos" OUTPUT="/path/to/jpg photos" QUALITY=95
make apple-test
```

Without Make, use the launcher directly:

```sh
bash scripts/mix-photo-export --runtime apple build
bash scripts/mix-photo-export --runtime apple convert \
  "/path/to/mix photos" "/path/to/jpg photos" --quality 95
bash scripts/mix-photo-export --runtime apple test
```

The launcher starts the Apple container system as needed. Builds use four CPUs
and 4 GiB of builder memory. Conversion uses two CPUs, 2 GiB of memory, no
network, the host UID/GID, read-only input, and a writable output bind mount.
Unlike the Docker path, the Apple path currently uses a writable ephemeral
root filesystem for temporary files. `--rm` removes the container after use.

The same commands can be issued directly:

```sh
container build --cpus 4 --memory 4g --progress plain -t mix-photo-export:local .
mkdir -p /absolute/path/to/jpg
container run --rm --progress none --network none --memory 2g \
  --user "$(id -u):$(id -g)" \
  --mount "type=bind,source=/absolute/path/to/mix,target=/input,readonly" \
  --mount "type=bind,source=/absolute/path/to/jpg,target=/output" \
  mix-photo-export:local -input /input -output /output
```

Docker and Apple containers have separate image stores. Building a tag with one
does not automatically make it available to the other; build once per runtime.
Each uses the same Linux source, dependencies, tests, and Dockerfile.

### Launcher Configuration

| Setting | Default | Purpose |
| --- | --- | --- |
| `MIX_RUNTIME` | `docker` | Default runtime; overridden by `--runtime` |
| `MIX_IMAGE` | `mix-photo-export:local` | Local image tag |
| Make `IMAGE` | `mix-photo-export:local` | Image tag passed to the launcher |
| Make `QUALITY` | `95` | Conversion quality |

For example, `make apple-build IMAGE=mix-photo-export:dev` creates a separate
development tag. Use the same tag for subsequent conversion/test commands.

## Command-Line Reference

Use the container launcher to select host folders and pass conversion options:

```sh
bash scripts/mix-photo-export --runtime docker convert \
  "/path/to/mix photos" "/path/to/jpg photos" --quality 98
docker run --rm --network none mix-photo-export:local --help
```

The options below are passed to `mix-photo-export` **inside the container**, after
the image name in a `docker run` or `container run` command:

| Option | Meaning |
| --- | --- |
| `-input PATH`, `--input PATH` | Source folder or individual file |
| Positional `PATH` | Backward-compatible alternative to `-input` |
| `-output DIR`, `--output DIR`, `-o DIR` | Destination directory, created if needed |
| `--quality N` | Integer 1 through 100; default 95 |
| `--help`, `-h` | Print app name and usage |

Only one input is accepted. Quote paths with spaces. Extensions are
case-insensitive. App paths refer to the container filesystem, not the host.
Always specify a writable output mount with `-output /output`; the input mount
is read-only. The launcher supplies both paths automatically.

The launcher's `convert` command accepts folders. To convert just one file,
use the direct container invocation shown above with the same folder mounts,
but replace `-input /input` with `-input "/input/photo.fpx"` or the desired
`.mix` filename. The file must exist inside the mounted source folder.

### Naming and Quality

The first discovered image is named `photo.jpg`. Additional stores are named
`photo-image-NNNNNN.jpg`, where the suffix is the document's storage identifier.
Stores are sorted by name, not ranked by photo size or visual importance.

Additional images can be photos, frames, or other design elements. Different
input files with the same basename can collide, so use separate destinations
in that case. Existing outputs are skipped by path, without checking their
contents. Choose a new folder to re-export at a different quality.

JPEG is lossy, including at quality 100. Output uses 8-bit RGB and 4:4:4 chroma
sampling. No upscaling occurs, and the app cannot restore detail beyond the
resolution stored in the source.

### Results and Errors

Each export prints its name and dimensions. A summary reports converted,
skipped, and failed counts. Exit code `0` means no reported conversion errors,
including successful skips; `1` indicates an argument or conversion error.
The launcher may return `2` for its own usage errors.

Folders with no matching images are errors. Ordinary per-image errors are
reported and the batch continues. Process crashes in the legacy library are not
caught, and there is no built-in per-file timeout.

## Testing

```sh
make docker-test
make apple-test
```

Both commands build the test stage and run regressions inside the selected
runtime. Without Make:

```sh
bash scripts/mix-photo-export --runtime docker test
bash scripts/mix-photo-export --runtime apple test
```

Default tests generate their own uncompressed RGBA FlashPix image and a MIX
document containing two image stores. They check conversion, full resolution,
RGB values, alpha flattening, output decoding, reruns, original hashes, invalid
inputs, and malformed arguments. JPEG validation uses TurboJPEG inside the
container. Generated fixtures and test exports stay in the disposable test
container, which is removed after the run.

To check your own archive, use either runtime's `convert` command with a fresh
output folder and inspect its exports. Source files remain mounted read-only.

The initial real-photo archive contained 29 documents with 36 embedded images.
Docker arm64 and Apple-container conversions completed successfully.
Generated tests also passed in the amd64 Alpine build under emulation. Synthetic
fixtures do not cover all compressed variants; real-photo tests provide additional
FlashPix JPEG coverage. No private photos are included in CI or image builds.

[CI](.github/workflows/build.yml) includes Alpine amd64/arm64 image builds and
container checks. A workflow being configured does not mean every hosted
job has already been run or passed.

## Implementation

All steps below run inside the container:

1. Copy a source file into a uniquely created temporary directory.
2. Enumerate compound-file storages using the toolkit's structured-storage API.
3. Copy each recognized `Data Object Store NNNNNN` into a standalone temporary
   FlashPix file, preserving streams and its class identifier.
4. Decode the highest stored resolution into RGBA pixels.
5. Flatten premultiplied transparency onto white and encode JPEG with TurboJPEG.
6. Close handles and remove temporary files on normal completion or handled errors.

When no recognized stores are found, the copied root is tried as a standalone
FlashPix image. Working on copies is important because the public toolkit API
opens images in modification mode internally. Standalone extraction avoids an
observed upstream `OLEFile::Release()` cleanup crash for directly opened
embedded images; the upstream source is not patched.

The repository vendors the complete FlashPix source at revision
`bb28e42245701ef5473ac086e06d505491ad3d8a` under
[third_party/libfpx](third_party/libfpx). CMake builds this local copy as C++98
and the app as C++17, without fetching upstream source or requiring a submodule.
Generated dependency files stay in the build directory. Container compilation
and regression tests run with networking disabled.

See [third_party/README.md](third_party/README.md) for provenance, license
notices, and the update procedure. Vendoring protects against upstream source
disappearance, not future security issues or compiler incompatibilities.
TurboJPEG and the other build dependencies are installed inside the image
stages, not on the host. Alpine tags and package versions are not locked to
exact digests, so builds are not claimed to be bit-for-bit reproducible.

| File | Purpose |
| --- | --- |
| [mix_photo_export.cpp](mix_photo_export.cpp) | CLI, temporary copies, image conversion, batch reporting |
| [storage.cpp](storage.cpp) | Compound storage discovery and extraction |
| [CMakeLists.txt](CMakeLists.txt) | Compilation and test configuration used by the image build |
| [Dockerfile](Dockerfile) | Shared Alpine build/test/runtime stages |
| [scripts/mix-photo-export](scripts/mix-photo-export) | Docker/Apple runtime selection and folder mounts |
| [Makefile](Makefile) | Docker and Apple-container convenience targets |
| [tests/fixtures.cpp](tests/fixtures.cpp) | Generated images and portable JPEG validation |
| [tests/regression.cmake](tests/regression.cmake) | End-to-end regression checks |
| [third_party/README.md](third_party/README.md) | Vendored libfpx provenance, licensing, and maintenance |

## Troubleshooting

| Problem | Check |
| --- | --- |
| Missing runtime library | Rebuild the image with the supplied Dockerfile; dependencies belong in the image, not on the host. |
| Docker daemon unavailable | Start Docker Desktop/Engine; on Windows select Linux-container mode. |
| Apple API server unavailable | Run `container system start` and check the installed CLI's OS requirements. |
| Image not found in one runtime | Docker and Apple use separate stores; build using the selected runtime. |
| Bind mount denied | Check Docker Desktop file sharing or macOS folder permissions. |
| Permission denied writing JPEGs | Check the destination; use the launcher to match host UID/GID on Unix hosts. |
| No matching files | Check the path, extensions, and nonrecursive scanning. |
| Outputs skipped | Use a fresh output folder to repeat conversion. |
| Unsupported dimensions | The app limits each side to 65,500 pixels and total area to 100 million pixels. |
| Out of memory or temporary space | Allow for the full RGBA image, decoder buffers, copies, and JPEG output; adjust container limits for large inputs. |
| FlashPix/OLE error | Retain the source and error code; the document may be damaged or an unsupported variant. |
| Photos differ from Picture It! | Document composition and viewing transforms are not rendered. |

Use trusted photo archives and keep backups. Container isolation and read-only
input mounts reduce exposure, but are not a guarantee against vulnerabilities
in this legacy parser. Temporary copies stay in the container; exported JPEGs
remain in the host output folder after the container is removed. JPEGs are not
substitutes for editable originals.

## Licensing

The FlashPix toolkit originates from the Digital Imaging Group, Inc. and Eastman
Kodak Company and is maintained by ImageMagick Studio LLC. The vendored source
retains its upstream notices; review
[third_party/README.md](third_party/README.md) and
[third_party/libfpx/flashpix.h](third_party/libfpx/flashpix.h) before
redistribution. The runtime image includes the notice from `flashpix.h` and
the dependency provenance document; system packages have their own license terms.

This product includes software developed by the contributors and Digital
Imaging Group, Inc. (<http://www.digitalimaging.org/>) for use in the Flashpix
Toolkit Project.

libjpeg-turbo and Alpine packages are third-party dependencies under their own
licenses. This repository currently has no separate license for the app's own
source; dependency licenses do not automatically license it.
