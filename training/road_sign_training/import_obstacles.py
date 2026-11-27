"""Import a reviewed, pinned ROD obstacle subset without changing source files."""

import argparse
import hashlib
import json
import math
import shutil
import tempfile
from pathlib import Path

from road_sign_training.dataset_manifest import require_object, require_text

OBSTACLE_CLASS_IDS = frozenset({6, 9, 13, 17, 18, 20, 22, 24})
SPLITS = frozenset({"train", "valid", "test"})


def source_file(root: Path, raw_path: object, checksum: object) -> Path:
    relative = Path(require_text(raw_path, "path"))
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError("path: must remain inside source root")
    path = root / relative
    if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root.resolve()):
        raise ValueError("path: missing, linked or outside source root")
    if not isinstance(checksum, str) or len(checksum) != 64:
        raise ValueError("sha256: expected SHA-256")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != checksum:
        raise ValueError("sha256: source checksum mismatch")
    return path


def parse_labels(path: Path) -> tuple[list[list[float]], list[list[float]]]:
    obstacle_boxes: list[list[float]] = []
    ignored_boxes: list[list[float]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        fields = line.split()
        if len(fields) != 5:
            raise ValueError("label: expected class and four YOLO coordinates")
        try:
            class_id = int(fields[0])
            x, y, width, height = (float(value) for value in fields[1:])
        except ValueError as error:
            raise ValueError("label: invalid class or coordinates") from error
        if not 0 <= class_id < 25 or not all(
            math.isfinite(value) and 0 <= value <= 1 for value in (x, y, width, height)
        ):
            raise ValueError("label: invalid class or coordinates")
        if width <= 0 or height <= 0 or x - width / 2 < 0 or x + width / 2 > 1:
            raise ValueError("label: box outside image")
        if y - height / 2 < 0 or y + height / 2 > 1:
            raise ValueError("label: box outside image")
        box = [float(class_id), x, y, width, height]
        if class_id in OBSTACLE_CLASS_IDS:
            obstacle_boxes.append(box)
        else:
            ignored_boxes.append(box)
    return obstacle_boxes, ignored_boxes


def import_obstacles(source_root: Path, review_path: Path, output_dir: Path) -> dict[str, object]:
    review = require_object(
        json.loads(review_path.read_text(encoding="utf-8")),
        {"datasetRevision", "images"},
        "review",
    )
    revision = require_text(review["datasetRevision"], "datasetRevision")
    if len(revision) != 40 or any(char not in "0123456789abcdef" for char in revision):
        raise ValueError("datasetRevision: expected pinned commit SHA")
    images = review["images"]
    if not isinstance(images, list) or not images:
        raise ValueError("images: expected nonempty list")
    if output_dir.exists():
        raise ValueError("output: directory already exists")

    scenes: list[dict[str, object]] = []
    copies: list[tuple[Path, str]] = []
    groups: dict[tuple[str, str], str] = {}
    for index, raw_image in enumerate(images):
        image = require_object(
            raw_image,
            {"imagePath", "imageSha256", "labelPath", "labelSha256", "sourceId",
             "locationGroup", "split", "permission", "licence", "approved",
             "reviewedCompleteAnnotations"},
            "image",
        )
        if image["approved"] is not True or image["reviewedCompleteAnnotations"] is not True:
            raise ValueError("image: source rights and complete annotations require review")
        source_id = require_text(image["sourceId"], "sourceId")
        location = require_text(image["locationGroup"], "locationGroup")
        permission = require_text(image["permission"], "permission")
        licence = require_text(image["licence"], "licence")
        split = image["split"]
        if not isinstance(split, str) or split not in SPLITS:
            raise ValueError("split: invalid ROD split")
        for key in (("source", source_id), ("location", location)):
            if key in groups and groups[key] != split:
                raise ValueError("split: source/location leakage")
            groups[key] = split
        media = source_file(source_root, image["imagePath"], image["imageSha256"])
        label = source_file(source_root, image["labelPath"], image["labelSha256"])
        boxes, ignored_boxes = parse_labels(label)
        if not boxes:
            raise ValueError("image: no approved obstacle annotation")
        target_name = f"{index:06d}{media.suffix.lower()}"
        copies.append((media, target_name))
        scenes.append({
            "image": target_name,
            "imageSha256": image["imageSha256"],
            "labelSha256": image["labelSha256"],
            "sourceId": source_id,
            "locationGroup": location,
            "split": split,
            "permission": permission,
            "licence": licence,
            "obstacleBoxesYolo": boxes,
            "ignoredBoxesYolo": ignored_boxes,
            "requiresReview": bool(ignored_boxes),
        })

    result: dict[str, object] = {"datasetRevision": revision, "scenes": scenes}
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output_dir.parent) as temporary:
        staging = Path(temporary)
        for source, name in copies:
            shutil.copyfile(source, staging / name)
        (staging / "subset.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        staging.rename(output_dir)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--review", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = import_obstacles(args.source_root, args.review, args.output)
    scenes = result["scenes"]
    assert isinstance(scenes, list)
    print(f"Imported {len(scenes)} reviewed scenes into {args.output}")


if __name__ == "__main__":
    main()
