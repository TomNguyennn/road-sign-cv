"""Validated, offline-only research dataset manifests."""

import hashlib
import json
import math
import random
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

SPLITS = ("train", "validation", "test")
SPLIT_FRACTIONS = {"train": 0.70, "validation": 0.15, "test": 0.15}
SPLIT_SEED = 42
CLASS_COUNT = 8
TEXT_CLASS_IDS = frozenset({4, 6})
CATALOGUE_PATH = Path(__file__).resolve().parents[2] / "resources" / "catalogue.json"


@dataclass(frozen=True)
class Annotation:
    class_id: int
    box: tuple[float, float, float, float]
    transcript: str | None
    track_id: str | None


@dataclass(frozen=True)
class Scene:
    scene_id: str
    source_id: str
    location_group: str
    split: str
    media_path: Path
    media_kind: str
    width: int
    height: int
    condition_tags: tuple[str, ...]
    annotations: tuple[Annotation, ...]


@dataclass(frozen=True)
class DatasetManifest:
    dataset_id: str
    version: str
    catalogue_sha256: str
    scenes: tuple[Scene, ...]


def require_object(value: object, fields: set[str], context: str) -> dict[str, object]:
    if not isinstance(value, dict) or not fields.issubset(value):
        raise ValueError(f"{context}: missing required fields")
    if any(not isinstance(key, str) for key in value):
        raise ValueError(f"{context}: field names must be text")
    return value


def require_text(value: object, context: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{context}: expected nonempty text")
    return value


def require_positive_int(value: object, context: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ValueError(f"{context}: expected positive integer")
    return value


def group_classes(group: Sequence[str], scenes: Mapping[str, Mapping[str, object]]) -> set[int]:
    classes: set[int] = set()
    for scene_id in group:
        annotations = scenes[scene_id].get("annotations", [])
        if isinstance(annotations, list):
            for annotation in annotations:
                if isinstance(annotation, dict) and isinstance(annotation.get("classId"), int):
                    classes.add(annotation["classId"])
    return classes


def assign_grouped_splits(scenes: Sequence[Mapping[str, object]]) -> dict[str, str]:
    """Assign connected source/location groups with a fixed seed and 70/15/15 targets."""
    parents = {
        require_text(scene["sceneId"], "sceneId"): require_text(scene["sceneId"], "sceneId")
        for scene in scenes
    }

    def root(scene_id: str) -> str:
        while parents[scene_id] != scene_id:
            parents[scene_id] = parents[parents[scene_id]]
            scene_id = parents[scene_id]
        return scene_id

    owners: dict[tuple[str, str], str] = {}
    for scene in scenes:
        scene_id = require_text(scene["sceneId"], "sceneId")
        for kind, field in (("source", "sourceId"), ("location", "locationGroup")):
            key = (kind, require_text(scene[field], field))
            if key in owners:
                parents[root(scene_id)] = root(owners[key])
            owners[key] = scene_id

    groups: dict[str, list[str]] = {}
    for scene_id in parents:
        groups.setdefault(root(scene_id), []).append(scene_id)
    scene_by_id = {require_text(scene["sceneId"], "sceneId"): scene for scene in scenes}
    ordered_groups = sorted(groups.values(), key=lambda group: min(group))
    random.Random(SPLIT_SEED).shuffle(ordered_groups)
    categories = {min(group): group_classes(group, scene_by_id) for group in ordered_groups}
    frequency = {
        class_id: sum(class_id in group_categories for group_categories in categories.values())
        for class_id in range(CLASS_COUNT)
    }
    ordered_groups.sort(
        key=lambda group: sum(1 / frequency[class_id] for class_id in categories[min(group)]),
        reverse=True,
    )
    targets = {split: len(scenes) * fraction for split, fraction in SPLIT_FRACTIONS.items()}
    counts = dict.fromkeys(SPLITS, 0)
    coverage: dict[str, set[int]] = {split: set() for split in SPLITS}
    assigned: dict[str, str] = {}
    for group in ordered_groups:
        group_categories = categories[min(group)]
        split = max(
            SPLITS,
            key=lambda candidate: (
                sum(1 / frequency[class_id] for class_id in group_categories - coverage[candidate]),
                targets[candidate] - counts[candidate],
            ),
        )
        counts[split] += len(group)
        coverage[split].update(group_categories)
        assigned.update({scene_id: split for scene_id in group})
    return assigned


def parse_annotation(
    value: object, width: int, height: int, media_kind: str, categories: tuple[str, ...]
) -> Annotation:
    item = require_object(
        value, {"classId", "category", "box", "transcript", "trackId"}, "annotation"
    )
    class_id = item["classId"]
    if (
        isinstance(class_id, bool)
        or not isinstance(class_id, int)
        or not 0 <= class_id < CLASS_COUNT
    ):
        raise ValueError("annotation: invalid classId")
    if item["category"] != categories[class_id]:
        raise ValueError("annotation: category does not match catalogue classId")
    raw_box = item["box"]
    if not isinstance(raw_box, list) or len(raw_box) != 4:
        raise ValueError("annotation: box must be xywh")
    if any(isinstance(number, bool) or not isinstance(number, (int, float)) for number in raw_box):
        raise ValueError("annotation: box must contain numbers")
    x, y, box_width, box_height = (float(number) for number in raw_box)
    if not all(math.isfinite(number) for number in (x, y, box_width, box_height)):
        raise ValueError("annotation: box must be finite")
    if (
        x < 0
        or y < 0
        or box_width <= 0
        or box_height <= 0
        or x + box_width > width
        or y + box_height > height
    ):
        raise ValueError("annotation: box outside image")
    transcript = item["transcript"]
    if transcript is not None:
        transcript = require_text(transcript, "transcript")
    if class_id not in TEXT_CLASS_IDS and transcript is not None:
        raise ValueError("annotation: non-text class cannot carry transcript")
    track_id = item["trackId"]
    if track_id is not None:
        track_id = require_text(track_id, "trackId")
    if media_kind == "video-frame" and track_id is None:
        raise ValueError("annotation: video-frame requires trackId")
    return Annotation(class_id, (x, y, box_width, box_height), transcript, track_id)


def validate_media(path: Path, raw_path: object, checksum: object) -> Path:
    relative = Path(require_text(raw_path, "mediaPath"))
    root = path.parent.resolve()
    media = (root / relative).resolve()
    if relative.is_absolute() or not media.is_relative_to(root) or not media.is_file():
        raise ValueError("mediaPath: missing or outside dataset")
    if (
        not isinstance(checksum, str)
        or len(checksum) != 64
        or any(char not in "0123456789abcdef" for char in checksum)
    ):
        raise ValueError("sha256: expected lowercase SHA-256")
    digest = hashlib.sha256()
    with media.open("rb") as content:
        for chunk in iter(lambda: content.read(1024 * 1024), b""):
            digest.update(chunk)
    if digest.hexdigest() != checksum:
        raise ValueError("sha256: media checksum mismatch")
    return media


def parse_scene(value: object, path: Path, categories: tuple[str, ...]) -> Scene:
    item = require_object(
        value,
        {
            "sceneId",
            "sourceId",
            "locationGroup",
            "split",
            "mediaPath",
            "sha256",
            "mediaKind",
            "width",
            "height",
            "conditionTags",
            "annotations",
        },
        "scene",
    )
    scene_id = require_text(item["sceneId"], "sceneId")
    source_id = require_text(item["sourceId"], "sourceId")
    location_group = require_text(item["locationGroup"], "locationGroup")
    split = item["split"]
    if not isinstance(split, str) or split not in SPLITS:
        raise ValueError("scene: invalid split")
    media_kind = item["mediaKind"]
    if not isinstance(media_kind, str) or media_kind not in ("image", "video-frame"):
        raise ValueError("scene: invalid mediaKind")
    width = require_positive_int(item["width"], "width")
    height = require_positive_int(item["height"], "height")
    tags = item["conditionTags"]
    if (
        not isinstance(tags, list)
        or not tags
        or any(not isinstance(tag, str) or not tag.strip() for tag in tags)
    ):
        raise ValueError("conditionTags: expected nonempty text list")
    raw_annotations = item["annotations"]
    if not isinstance(raw_annotations, list):
        raise ValueError("annotations: expected list")
    if "augmentation" in item:
        augmentations = item["augmentation"]
        if split != "train" or not isinstance(augmentations, list):
            raise ValueError("augmentation: allowed only in training scenes")
        if any(not isinstance(name, str) or not name.strip() for name in augmentations):
            raise ValueError("augmentation: expected text names")
    media = validate_media(path, item["mediaPath"], item["sha256"])
    annotations = tuple(
        parse_annotation(value, width, height, media_kind, categories) for value in raw_annotations
    )
    return Scene(
        scene_id,
        source_id,
        location_group,
        split,
        media,
        media_kind,
        width,
        height,
        tuple(tags),
        annotations,
    )


def load_manifest(path: Path) -> DatasetManifest:
    raw = require_object(
        json.loads(path.read_text(encoding="utf-8")),
        {"datasetId", "version", "catalogueSha256", "seed", "sources", "scenes"},
        "manifest",
    )
    catalogue_bytes = CATALOGUE_PATH.read_bytes()
    catalogue_sha256 = hashlib.sha256(catalogue_bytes).hexdigest()
    if raw["catalogueSha256"] != catalogue_sha256:
        raise ValueError("catalogueSha256: catalogue checksum mismatch")
    catalogue = json.loads(catalogue_bytes)
    categories = tuple(entry["category"] for entry in catalogue["entries"])
    if len(categories) != CLASS_COUNT:
        raise ValueError("catalogue: invalid class count")
    if raw["seed"] != SPLIT_SEED or isinstance(raw["seed"], bool):
        raise ValueError("seed: expected 42")
    sources = raw["sources"]
    raw_scenes = raw["scenes"]
    if not isinstance(sources, list) or not isinstance(raw_scenes, list) or not raw_scenes:
        raise ValueError("manifest: sources and scenes must be nonempty lists")
    source_ids: set[str] = set()
    for value in sources:
        source = require_object(value, {"sourceId", "permission", "usageTerms"}, "source")
        source_id = require_text(source["sourceId"], "sourceId")
        require_text(source["permission"], "permission")
        require_text(source["usageTerms"], "usageTerms")
        if source_id in source_ids:
            raise ValueError("sourceId: duplicate")
        source_ids.add(source_id)
    scenes = tuple(parse_scene(value, path, categories) for value in raw_scenes)
    if len({scene.scene_id for scene in scenes}) != len(scenes):
        raise ValueError("sceneId: duplicate")
    if any(scene.source_id not in source_ids for scene in scenes):
        raise ValueError("scene: unknown sourceId")
    assignments = assign_grouped_splits(raw_scenes)
    if any(scene.split != assignments[scene.scene_id] for scene in scenes):
        raise ValueError("split: inconsistent with deterministic grouped assignment")
    for split in SPLITS:
        classes = {
            annotation.class_id
            for scene in scenes
            if scene.split == split
            for annotation in scene.annotations
        }
        if classes != set(range(CLASS_COUNT)):
            raise ValueError(f"{split}: missing category coverage")
    if not any(not scene.annotations for scene in scenes):
        raise ValueError("scenes: at least one negative is required")
    return DatasetManifest(
        require_text(raw["datasetId"], "datasetId"),
        require_text(raw["version"], "version"),
        catalogue_sha256,
        scenes,
    )
