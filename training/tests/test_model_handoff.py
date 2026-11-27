"""Checks for the shared detector catalogue contract."""

import json
from pathlib import Path

from road_sign_training.dataset_manifest import CLASS_COUNT, TEXT_CLASS_IDS

CATALOGUE_PATH = Path(__file__).resolve().parents[2] / "resources" / "catalogue.json"
POLICY_PATH = Path(__file__).resolve().parents[2] / "config" / "policy.json"


def test_catalogue_matches_model_class_order() -> None:
    catalogue = json.loads(CATALOGUE_PATH.read_text(encoding="utf-8"))
    entries = catalogue["entries"]
    expected = [
        ("pedestrian-crossing-warning", "Pedestrian crossing warning", False, 0),
        ("children-warning", "Children warning", False, 0),
        ("roadworks-warning", "Roadworks warning", False, 0),
        ("no-pedestrians", "No pedestrians", False, 0),
        ("street-name", "Street name", True, 2),
        ("shared-path-with-cyclist", "Shared path with cyclists", False, 1),
        ("bus-stop", "Bus stop", True, 2),
        ("obstacles", "Possible obstacle", False, 0),
    ]

    assert catalogue["schemaVersion"] == 1
    assert catalogue["catalogueVersion"] == 2
    assert len(entries) == CLASS_COUNT
    assert {entry["classId"] for entry in entries if entry["isTextRelevant"]} == TEXT_CLASS_IDS
    assert len(entries) == len(expected)
    for class_id, (category, spoken_name, is_text_relevant, priority) in enumerate(expected):
        entry = entries[class_id]
        assert entry["classId"] == class_id
        assert entry["category"] == category
        assert entry["spokenName"] == spoken_name
        assert entry["isTextRelevant"] is is_text_relevant
        assert entry["priority"] == priority
        assert entry["includedVariants"]
        assert entry["excludedVariants"]

    policy = json.loads(POLICY_PATH.read_text(encoding="utf-8"))
    assert policy["speech"]["categoryPriority"] == [entry["priority"] for entry in entries]
