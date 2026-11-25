"""Checks for the shared detector catalogue contract."""

import json
from pathlib import Path

CATALOGUE_PATH = Path(__file__).resolve().parents[2] / "resources" / "catalogue.json"


def test_catalogue_matches_model_class_order() -> None:
    catalogue = json.loads(CATALOGUE_PATH.read_text(encoding="utf-8"))
    entries = catalogue["entries"]
    expected = [
        ("pedestrian-crossing-warning", "Pedestrian crossing warning", False, 0),
        ("children-warning", "Children warning", False, 0),
        ("roadworks-warning", "Roadworks warning", False, 0),
        ("no-pedestrians", "No pedestrians", False, 0),
        ("street-name", "Street name", True, 2),
        ("pedestrian-direction", "Pedestrian direction sign", True, 1),
        ("bus-stop", "Bus stop", True, 2),
        ("temporary-pedestrian-notice", "Temporary pedestrian notice", True, 0),
    ]

    assert catalogue["schemaVersion"] == 1
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
