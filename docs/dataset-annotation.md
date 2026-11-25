# Dataset annotation rules

Catalogue version 1 is [`resources/catalogue.json`](../resources/catalogue.json). Keep its eight class IDs and order identical to `contracts/model-bundle.md`; a changed class definition needs a new catalogue version and retraining. The catalogue defines detector classes, not every possible UK sign.

## Label each scene

- Record the source, permission and usage terms before annotation. Group images and video from the same physical location or source collection together for splitting. Demo uploads are never research data by default.
- Draw a tight box around the visible sign face in upright image coordinates. Use one box per physical sign. Clip a partially visible sign to the image edge; do not extend a box into unseen pixels. Do not annotate a blurred sign whose category cannot be identified.
- Give video instances a stable physical track ID within a source clip. Distinct signs keep distinct IDs even when their text matches. Record lighting, motion, viewing angle and image-quality tags for evaluation.
- Use exactly one class from the catalogue for each box. If two supported signs are physically distinct, box each separately. If a category is ambiguous, mark the example for adjudication and keep it out of training until resolved.
- Transcribe only visible, readable text for classes 4–7. Preserve observed spelling and punctuation; mark obscured or unreadable text as unavailable, without guessing. Classes 0–3 carry symbol meaning only. A separate attached plate gets a text-class box only when it independently fits that class.

## Class boundaries

| Class | Include | Exclude or resolve separately |
| --- | --- | --- |
| 0 Pedestrian crossing warning | Crossing pedestrian warning symbol | Crossing lights, road markings and generic wayfinding |
| 1 Children warning | Children warning symbol | School nameplates without that symbol |
| 2 Roadworks warning | Roadworks worker warning symbol | Worksites without a sign and text-only notices |
| 3 No pedestrians | Pedestrian prohibition symbol | Vehicle restrictions and informal handwritten requests |
| 4 Street name | Fixed plate naming a street | Business names, house numbers and maps |
| 5 Pedestrian direction | Fixed pedestrian wayfinding with destination and direction marker | Vehicle-only directions and temporary diversions |
| 6 Bus stop | Bus stop identification sign and attached stop-name plate | Buses, advertisements and standalone timetable sheets |
| 7 Temporary pedestrian notice | Posted temporary closure, diversion or access notice explicitly concerning pedestrians | Ordinary fixed direction signs, advertising and unreadable boards |

For class 7, record the wording that is actually legible. Do not infer that an arbitrary temporary board prohibits passage. If a sign could be class 5 or 7, use class 7 only when it is a temporary pedestrian access or diversion notice; otherwise use class 5 only when it is fixed wayfinding with an explicit direction marker.

## Negatives and review

Include scene-level negatives with no supported sign, plus hard negatives such as vehicle-only signs, shop fronts, maps, construction scenes without warning signs, blurred boards and unsupported signs. Label them as negatives, not as an extra detector class. Keep a reviewed list of ambiguous examples and the reason for exclusion or reassignment. Evaluate each category and each condition tag on held-out real scenes; artwork or synthetic fixtures do not establish real-world accuracy.

Speech may state the accepted sign meaning and quote accepted OCR text. It must not turn an arrow, distance, destination, warning or unreadable text into a navigation instruction or crossing decision. If text is not accepted, omit that text rather than inventing a replacement.

## Manifest and validation

The offline `manifest.json` has `datasetId`, `version`, `seed: 42`, `sources` and `scenes`. Each source records `sourceId`, a human-reviewed `permission` record and `usageTerms`. Each scene records `sceneId`, `sourceId`, `locationGroup`, `split`, `mediaKind` (`image` or `video-frame`), a relative `mediaPath`, its lowercase SHA-256, original `width`/`height`, `conditionTags`, and `annotations`. Each annotation records `classId`, an upright-pixel `box` in `[x, y, width, height]` form, `transcript` (text or `null`) and `trackId` (text or `null`; required for video frames). An empty annotations list marks a negative scene.

Run `PYTHONPATH=training training/.venv/bin/python -m road_sign_training.validate_dataset --manifest <path>` before training. `assign_grouped_splits` gives a reproducible seed-42 assignment with 70/15/15 scene-count targets. All scenes connected by a shared source or location stay in one split; impossible exact ratios never justify splitting a group. The validator checks all eight classes in each split and requires negatives. If these checks fail, collect or annotate more authorised scenes. Apply any augmentation only to the training split after validation; validation and test media stay unchanged.
