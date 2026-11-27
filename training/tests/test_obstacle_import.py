import hashlib
import json
from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from road_sign_training.import_obstacles import OBSTACLE_CLASS_IDS, import_obstacles, parse_labels


def make_review(tmp_path: Path) -> Path:
    source = tmp_path / "source"
    source.mkdir()
    (source / "image.jpg").write_bytes(b"fixture image")
    (source / "label.txt").write_text("6 0.5 0.5 0.2 0.2\n2 0.2 0.2 0.1 0.1\n")
    review = {
        "datasetRevision": "a" * 40,
        "images": [
            {
                "imagePath": "image.jpg",
                "imageSha256": hashlib.sha256((source / "image.jpg").read_bytes()).hexdigest(),
                "labelPath": "label.txt",
                "labelSha256": hashlib.sha256((source / "label.txt").read_bytes()).hexdigest(),
                "sourceId": "reviewed-source",
                "locationGroup": "same-place",
                "split": "train",
                "permission": "fixture owner approved training",
                "licence": "fixture only",
                "approved": True,
                "reviewedCompleteAnnotations": True,
            }
        ],
    }
    path = tmp_path / "review.json"
    path.write_text(json.dumps(review))
    return path


def test_import_preserves_provenance_and_flags_dropped_labels(tmp_path: Path) -> None:
    review_path = make_review(tmp_path)
    before = (tmp_path / "source" / "image.jpg").read_bytes()
    result = import_obstacles(tmp_path / "source", review_path, tmp_path / "output")
    scenes = result["scenes"]
    assert isinstance(scenes, list)
    scene = scenes[0]
    assert isinstance(scene, dict)
    assert scene["sourceId"] == "reviewed-source"
    assert scene["obstacleBoxesYolo"] == [[7.0, 0.5, 0.5, 0.2, 0.2]]
    assert scene["sourceObstacleClassIds"] == [6]
    assert scene["ignoredBoxesYolo"] == [[2.0, 0.2, 0.2, 0.1, 0.1]]
    assert scene["requiresReview"] is True
    assert (tmp_path / "output" / "000000.jpg").read_bytes() == before
    assert (tmp_path / "source" / "image.jpg").read_bytes() == before
    assert json.loads((tmp_path / "output" / "subset.json").read_text()) == result


@pytest.mark.parametrize("source_class_id", sorted(OBSTACLE_CLASS_IDS))
def test_maps_each_approved_rod_class_to_obstacle(tmp_path: Path, source_class_id: int) -> None:
    label = tmp_path / "label.txt"
    label.write_text(f"{source_class_id} 0.5 0.5 0.2 0.2\n", encoding="utf-8")
    boxes, original_ids, ignored = parse_labels(label)
    assert boxes == [[7.0, 0.5, 0.5, 0.2, 0.2]]
    assert original_ids == [source_class_id]
    assert ignored == []


@pytest.mark.parametrize(
    "change",
    [
        lambda value: value.update(datasetRevision="latest"),
        lambda value: value["images"][0].update(approved=False),
        lambda value: value["images"][0].update(imageSha256="0" * 64),
        lambda value: value["images"][0].update(imagePath="../other.jpg"),
    ],
)
def test_rejects_unreviewed_or_unpinned_inputs(
    tmp_path: Path, change: Callable[[dict[str, Any]], None]
) -> None:
    review_path = make_review(tmp_path)
    review = json.loads(review_path.read_text())
    change(review)
    review_path.write_text(json.dumps(review))
    with pytest.raises(ValueError):
        import_obstacles(tmp_path / "source", review_path, tmp_path / "output")
    assert not (tmp_path / "output").exists()


def test_rejects_split_leakage_and_invalid_boxes(tmp_path: Path) -> None:
    review_path = make_review(tmp_path)
    review = json.loads(review_path.read_text())
    second = dict(review["images"][0], split="test")
    review["images"].append(second)
    review_path.write_text(json.dumps(review))
    with pytest.raises(ValueError, match="leakage"):
        import_obstacles(tmp_path / "source", review_path, tmp_path / "output")

    review["images"].pop()
    (tmp_path / "source" / "label.txt").write_text("6 0.95 0.5 0.2 0.2\n")
    review["images"][0]["labelSha256"] = hashlib.sha256(
        (tmp_path / "source" / "label.txt").read_bytes()
    ).hexdigest()
    review_path.write_text(json.dumps(review))
    with pytest.raises(ValueError, match="outside image"):
        import_obstacles(tmp_path / "source", review_path, tmp_path / "output")
