"""Validate an offline research dataset before training."""

import argparse
from pathlib import Path

from road_sign_training.dataset_manifest import load_manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    print(
        f"Validated {manifest.dataset_id} version {manifest.version}: {len(manifest.scenes)} scenes"
    )


if __name__ == "__main__":
    main()
