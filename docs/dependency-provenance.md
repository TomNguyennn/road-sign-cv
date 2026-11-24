# Dependency provenance

The machine-readable inventory is `config/dependency-manifest.json`. It records
source URLs, SHA-256 pins, package lock hashes, package integrity values, installed
Python metadata and the tested toolchains. `docs/third-party/` preserves upstream
native licence texts and notices, plus the OCR and Ultralytics notices. These are
upstream materials, not a new licence for this repository.

## Native sources

All thirteen native inputs are pinned in `cmake/Dependencies.cmake` and
`cmake/OnnxRuntime.cmake`. The manifest records their exact versions or commits.
ByteTrack uses commit `d1bf0191adff59bc8fcfeaa0b33d3d1642552a99` and only the tracker
sources; our include-only patch does not replace its MIT notice.

| Component | Upstream licence information |
|---|---|
| OpenCV, Tesseract, Sonic | Apache-2.0; preserve additional bundled notices |
| ONNX Runtime | MIT and package ThirdPartyNotices.txt |
| Drogon, Trantor, ByteTrack | MIT |
| JsonCpp | Public-domain dedication where recognised, with MIT alternative |
| Eigen | MPL-2.0 core, with other notices for particular bundled files |
| GoogleTest | BSD-3-Clause |
| Leptonica | BSD-style terms in leptonica-license.txt |
| FFmpeg | See its COPYING files and configured features; GPL/nonfree components are not enabled by our configuration |
| eSpeak NG | GPL-3.0 terms and additional bundled-data notices |

The copied top-level notices do not replace notices inside source archives or
installed packages. Packaging tasks must preserve relevant bundled notices in
redistributed images. This inventory does not establish that all dependencies
are permissively licensed or resolve distribution compatibility.

## OCR, speech and models

English OCR uses `tessdata_fast/eng.traineddata` from commit
`87416418657359cb625c412a48b6e1d6d41c29bd`, with its exact SHA-256 and Apache-2.0
notice recorded. The downloaded file is ignored local research/build material;
runtime model packaging is introduced later.

Speech selects eSpeak NG 1.52.0, voice `en-gb`, 160 words per minute. The manifest
records each generated voice-data file and its SHA-256 from the tested ARM64
installation. It is a build snapshot, not a claim that compiler-generated data
is identical on every platform. The source archive and Sonic commit are pinned.
The eSpeak licence applies independently of this project's future licence choice.

Ultralytics 8.4.218 includes AGPL-3.0 licence terms, retained in
`docs/third-party/ultralytics/LICENSE`. Training/export does not erase upstream
terms. No enterprise licence has been obtained through this implementation.
No custom detector weights, pretrained checkpoint or authorised training dataset
is included. Their identity, hashes, provenance and usage rights must be recorded
when the later dataset/training tasks obtain them; model release is not eligible.

## JavaScript and Python

`frontend/package-lock.json` is the installation authority for JavaScript,
including optional platform packages. Its SHA-256 and each package's registry
integrity value are recorded. npm audit reported zero known vulnerabilities for
the installed graph on 2026-11-24; this is not a permanent security guarantee.

`training/requirements.lock` pins the universal dependency graph with SHA-256
hashes and platform markers. The manifest's Python list describes the tested
macOS ARM64 installation, not every branch of the universal lock. Package licence
metadata can be absent or incomplete; the recorded installed notice paths and
hashed distribution archives remain the source for upstream terms. Python OpenCV
5.1.0.95 built from its hashed source on this host; the backend uses OpenCV 4.12.0.
Source build tools and OS libraries must also be captured when creating the
training image; the lock alone is not a fully reproducible OS image.

## Tested OS build prerequisites

Ubuntu 24.04 ARM64 image digest is pinned in the manifest. These apt package
versions were installed in the tested container; they are recorded observations,
not an apt repository snapshot or a completed deployment Dockerfile:

| Package | Version |
|---|---|
| gcc-13, g++-13 | 13.3.0-6ubuntu2~24.04.1 |
| cmake | 3.28.3-1build7 |
| ninja-build | 1.11.1-2 |
| make | 4.3-4.1build2 |
| pkg-config | 1.8.1-2build1 |
| nasm | 2.16.01-1build1 |
| libssl-dev | 3.0.13-0ubuntu3.16 |
| zlib1g-dev | 1:1.3.dfsg-3.1ubuntu2.3 |
| uuid-dev | 2.39.3-9ubuntu6.6 |
| libpng-dev | 1.6.43-5ubuntu0.7 |
| libjpeg-dev | 8c-2ubuntu11 |
| ca-certificates | 20260816ubuntu0.24.04.2 |

Preserve Ubuntu package copyright files during container packaging. Docker/Caddy,
Terraform/provider locks and GitHub action SHAs are recorded when their respective
tasks introduce those dependencies; no untested versions are represented here.
