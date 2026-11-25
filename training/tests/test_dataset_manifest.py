import hashlib
import json
from collections import Counter
from collections.abc import Callable
from pathlib import Path

import pytest

from road_sign_training.dataset_manifest import assign_grouped_splits, load_manifest


def make_manifest(tmp_path: Path) -> Path:
    sources = []
    scenes = []
    for index in range(20):
        media = tmp_path / f"scene-{index}.jpg"
        media.write_bytes(f"authorised fixture {index}".encode())
        source_id = f"source-{index}"
        sources.append(
            {"sourceId": source_id, "permission": "fixture owner", "usageTerms": "test only"}
        )
        annotations = []
        if index != 19:
            annotations = [
                {
                    "classId": class_id,
                    "box": [class_id * 10, 0, 8, 8],
                    "transcript": "Main Street" if class_id >= 4 else None,
                    "trackId": None,
                }
                for class_id in range(8)
            ]
        scenes.append(
            {
                "sceneId": f"scene-{index}",
                "sourceId": source_id,
                "locationGroup": f"location-{index}",
                "mediaKind": "image",
                "mediaPath": media.name,
                "sha256": hashlib.sha256(media.read_bytes()).hexdigest(),
                "width": 100,
                "height": 100,
                "conditionTags": ["daylight"],
                "annotations": annotations,
            }
        )
    assignments = assign_grouped_splits(scenes)
    for scene in scenes:
        scene_id = scene["sceneId"]
        assert isinstance(scene_id, str)
        scene["split"] = assignments[scene_id]
    manifest = {
        "datasetId": "fixture",
        "version": "1",
        "seed": 42,
        "sources": sources,
        "scenes": scenes,
    }
    path = tmp_path / "manifest.json"
    path.write_text(json.dumps(manifest), encoding="utf-8")
    return path


def rewrite_manifest(path: Path, edit: Callable[[dict[str, object]], None]) -> None:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    edit(manifest)
    path.write_text(json.dumps(manifest), encoding="utf-8")


def test_valid_grouped_manifest_with_negative(tmp_path: Path) -> None:
    path = make_manifest(tmp_path)
    manifest = load_manifest(path)
    assert len(manifest.scenes) == 20
    assert {scene.split for scene in manifest.scenes} == {"train", "validation", "test"}
    assert Counter(scene.split for scene in manifest.scenes) == {
        "train": 14,
        "validation": 3,
        "test": 3,
    }
    assert load_manifest(path).scenes == manifest.scenes


def test_shared_source_and_location_stay_in_one_split() -> None:
    scenes = [
        {"sceneId": "one", "sourceId": "source-a", "locationGroup": "place-a"},
        {"sceneId": "two", "sourceId": "source-a", "locationGroup": "place-b"},
        {"sceneId": "three", "sourceId": "source-b", "locationGroup": "place-b"},
    ]
    assignments = assign_grouped_splits(scenes)
    assert len(set(assignments.values())) == 1


def test_rare_category_is_stratified_across_splits() -> None:
    scenes = [
        {
            "sceneId": str(index),
            "sourceId": str(index),
            "locationGroup": str(index),
            "annotations": [{"classId": 7}] if index < 3 else [{"classId": 0}],
        }
        for index in range(20)
    ]
    assignments = assign_grouped_splits(scenes)
    assert {assignments[str(index)] for index in range(3)} == {"train", "validation", "test"}


@pytest.mark.parametrize(
    "change",
    [
        lambda value: value["sources"][0].update(permission=""),
        lambda value: value["scenes"][0]["annotations"][0].update(box=[-1, 0, 8, 8]),
        lambda value: value["scenes"][0]["annotations"][4].update(transcript=12),
        lambda value: value["scenes"][0].update(sha256="0" * 64),
        lambda value: value["scenes"][0].update(sourceId=value["scenes"][1]["sourceId"]),
        lambda value: value["scenes"][0].update(split="test"),
    ],
)
def test_rejects_invalid_or_leaking_manifest(
    tmp_path: Path, change: Callable[[dict[str, object]], None]
) -> None:
    path = make_manifest(tmp_path)
    rewrite_manifest(path, change)
    with pytest.raises(ValueError):
        load_manifest(path)


def test_video_track_and_train_only_augmentation(tmp_path: Path) -> None:
    path = make_manifest(tmp_path)
    manifest = json.loads(path.read_text(encoding="utf-8"))
    scene = next(scene for scene in manifest["scenes"] if scene["split"] == "validation")
    scene["augmentation"] = ["flip"]
    path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ValueError, match="augmentation"):
        load_manifest(path)

    scene.pop("augmentation")
    scene["mediaKind"] = "video-frame"
    path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ValueError, match="trackId"):
        load_manifest(path)


def test_rejects_missing_class_coverage_and_path_escape(tmp_path: Path) -> None:
    path = make_manifest(tmp_path)
    manifest = json.loads(path.read_text(encoding="utf-8"))
    for scene in manifest["scenes"]:
        scene["annotations"] = [
            annotation for annotation in scene["annotations"] if annotation["classId"] != 7
        ]
    path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ValueError, match="coverage"):
        load_manifest(path)

    path = make_manifest(tmp_path)
    manifest = json.loads(path.read_text(encoding="utf-8"))
    manifest["scenes"][0]["mediaPath"] = "../outside.jpg"
    path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ValueError, match="mediaPath"):
        load_manifest(path)
