# Model Candidate Handoff and Promotion (U4)

This contract connects local training in [model-bundle.md](model-bundle.md) to the trusted publishing
workflow in [azure-deployment.md](../azure-deployment.md). It adds no runtime Python or artifact
service. Ordinary user media, OCR text and session history never enter candidate packages.

## Candidate package and review input

After local training/export/parity succeeds, `training/road_sign_training/package_candidate.py`
produces explicit regular files for an OCI artifact of type
`application/vnd.road-sign.model-candidate.v1`. Its allowlisted inventory is:

- The existing bundle: detector.onnx, manifest.json, catalogue.json, policy.json and parity.json.
- package-manifest.json: schemaVersion=1, modelId, trainingGitSha, trainingCheckpointSha256,
  detectorSha256, tool versions and an exact list of relative file paths, sizes and SHA256 hashes.
  The file list covers every other file; the manifest's own hash is recorded in model selection.
- Authorised parity image fixtures and their manifest, including source/usage-permission references.
- Saved trained-framework reference tensors as little-endian FP32 `.f32` data with exact shape,
  byte count and fixture associations. Include the local framework-to-export parity report linking
  checkpoint, ONNX and fixture hashes. Do not package pickle checkpoints or executable Python.

Reject absolute/traversing paths, duplicates, symlinks, archives, executable scripts and undeclared
files. Bound a candidate to 256 files, 128 MiB per file and 512 MiB total uncompressed bytes; decoded
fixture images retain the normal pixel limits. Verify the OCI descriptor inventory and declared
sizes before pulling into a fresh directory, then verify actual bytes and every hash before model
loading. `.f32` shape/size must match the existing tensor contract; never deserialize object data.

The reviewed `config/model-selection.json` contains exactly schemaVersion=1, modelId,
candidateDigest (`sha256:` plus 64 lowercase hex digits) and packageManifestSha256 (64 lowercase
hex digits). The Azure registry and candidate repository `models/candidates` are fixed trusted
environment configuration; selection cannot supply a URL, registry, executable or arbitrary path.
The trainingGitSha is provenance and need not equal the later application commit that selects it.
Unknown fields or an invalid/missing selection block publication, not infrastructure bootstrap.

## First upload and repeatable retrieval

1. Apply the authorised bootstrap and gateway-disabled stack to create ACR and identities. No
   model selection or application release is required for this infrastructure-only operation.
2. The authorised trainer validates the local package, signs in interactively through Entra MFA
   (`az login`), and uses a short-lived ACR token with pinned ORAS through
   `scripts/deploy/upload-model-candidate.sh`. Send credentials over stdin, use a temporary credential
   directory, erase it on exit and never echo tokens. Push explicit files/media types, not a tarball
   of the working directory; capture the resulting OCI manifest digest and package-manifest hash.
3. Commit only the selection JSON through normal review. Keep weights, fixtures and local evidence
   out of Git. Approval of this selection establishes the trusted candidate and its expected hashes.
4. On a main push, after CI succeeds for that exact SHA, the protected publish job reads selection
   from that same checkout. A protected manual rerun may target an already-tested main SHA; it
   cannot substitute a different candidate through dispatch parameters. Use GitHub OIDC to pull
   `models/candidates@sha256:...`, never resolve a mutable tag at publish/deploy time.

Classic ACR RBAC is retained: the trainer has explicitly authorised registry-scoped AcrPush;
the publisher has registry-scoped AcrPush through OIDC; the VM has registry-scoped AcrPull through
managed identity. These roles are registry-wide trust, not repository isolation. Neither trainer
nor publisher receives infrastructure or VM-deployment rights. Use repository ABAC only through a
separate design change if that isolation is later required. Candidate prefixes are organization,
not an authorization boundary.
[ACR artifact authentication](https://learn.microsoft.com/en-us/azure/container-registry/container-registry-manage-artifact),
[ACR role scopes](https://learn.microsoft.com/en-us/azure/container-registry/container-registry-rbac-built-in-roles-directory-reference).

Inspect the remote OCI manifest before pull. Use a fresh output directory, do not enable path
traversal and reject overwrites. Verify the reviewed package-manifest hash, allowlisted inventory,
actual sizes and file hashes. Registry unavailability, deleted digest, unauthorized fixtures or
missing evidence fails closed; no fallback to latest, bundled generic weights or a developer path.
[ORAS manifest inspection](https://oras.land/docs/commands/oras_manifest_fetch/),
[ORAS pull](https://oras.land/docs/commands/oras_pull/).

## CI validation and deployment artifact

Untrusted/fork CI runs deterministic fixtures without registry credentials. Real-model validation
is a required trusted publishing gate: run the pinned Python ONNX and native pipeline against the
candidate's authorised fixtures, compare them with each other and the saved framework references
using model-bundle.md tolerances, and run real-model readiness/recognition tests. Verify that local
framework-to-export evidence matches the same detector/checkpoint/fixture hashes. CI does not claim
to have rerun training or to establish held-out model quality from parity fixtures.

Publish only the five deployment-bundle files as a separate OCI model artifact in `models/releases`;
parity images/reference tensors stay in the candidate artifact. Record candidateDigest,
packageManifestSha256, the resulting modelArtifactDigest, modelManifestSha256, tested gitSha and
validation evidence in the release manifest. Store that manifest and sanitized CI evidence as an
OCI release artifact in `releases/app`; emit its digest as the deployment input. Deploy/rollback
uses this digest to retrieve the manifest with managed identity and verifies all selected artifacts.
releaseId is its human-facing label, not a mutable artifact selector. Protected deployment must
verify the manifest gitSha and evidence correspond to the tested/reviewed release.

Retain and protect the active and previous successful release manifests, their image/model digests
and referenced candidate evidence against deletion; a tag alone does not make its manifest
immutable. Never put media, secrets or raw environment dumps in published evidence.
[ACR artifact locking](https://learn.microsoft.com/en-us/azure/container-registry/container-registry-image-lock).

## Acceptance

A fresh trusted checkout with only the reviewed selection must retrieve and validate the same
candidate without local weights. Test wrong digest/hash/modelId, extra files, malformed tensor sizes,
unsafe paths, missing permissions/evidence, missing/deleted artifacts and changed application SHA.
Verify fork isolation, first infrastructure apply without a model, initial operator upload, trusted
promotion, deployment-bundle exclusion of fixtures, and rollback while a newer candidate exists.
