# Dataset annotation rules

Catalogue version 2 is [`resources/catalogue.json`](../resources/catalogue.json). Keep its eight class IDs and order identical to `contracts/model-bundle.md`; a changed class definition needs a new catalogue version and retraining. The catalogue defines detector classes, not every possible UK sign. Record its exact SHA-256 in each dataset manifest so labels made under an older mapping cannot be reused silently.

## Label each scene

- Record the source, permission and usage terms before annotation. Group images and video from the same physical location or source collection together for splitting. Demo uploads are never research data by default.
- Draw a tight box around the visible sign face or obstacle in upright image coordinates. Use one box per physical object. Clip a partially visible object to the image edge; do not extend a box into unseen pixels. Do not annotate a blurred object whose category cannot be identified.
- Give video instances a stable physical track ID within a source clip. Distinct signs keep distinct IDs even when their text matches. Record lighting, motion, viewing angle and image-quality tags for evaluation.
- Use exactly one class from the catalogue for each box. If two supported signs are physically distinct, box each separately. If a category is ambiguous, mark the example for adjudication and keep it out of training until resolved.
- Transcribe only visible, readable text for classes 4 and 6. Preserve observed spelling and punctuation; mark obscured or unreadable text as unavailable, without guessing. Classes 0–3 and 5 carry symbol meaning only; class 7 has no OCR. A separate attached plate gets a text-class box only when it independently fits that class.

## Class boundaries

| Class | Include | Exclude or resolve separately |
| --- | --- | --- |
| 0 Pedestrian crossing warning | Crossing pedestrian warning symbol | Crossing lights, road markings and generic wayfinding |
| 1 Children warning | Children warning symbol | School nameplates without that symbol |
| 2 Roadworks warning | Roadworks worker warning symbol | Worksites without a sign and text-only notices |
| 3 No pedestrians | Pedestrian prohibition symbol | Vehicle restrictions and informal handwritten requests |
| 4 Street name | Fixed plate naming a street | Business names, house numbers and maps |
| 5 Shared path with cyclists | UK shared pedestrian and cycle path sign showing both symbols | Cycle-only or pedestrian-only path signs, text-only notices and cyclists without a sign |
| 6 Bus stop | Bus stop identification sign and attached stop-name plate | Buses, advertisements and standalone timetable sheets |
| 7 Obstacles | Visible fixed street furniture such as poles, bins, benches, bollards and fixed barriers; stationary cones or temporary barriers | People, animals, moving vehicles, ambiguous clutter and objects without reliable visible bounds |

For class 7, draw the visible obstacle bounds, not a guessed ground footprint. Its location in the image is an observation, not evidence of physical distance, collision risk or a safe route. Exclude objects whose motion or category is uncertain during review; do not relabel an unreviewed dropped class as confirmed background.

## Negatives and review

Include scene-level negatives with no supported class, plus hard negatives such as vehicle-only signs, shop fronts, maps, construction scenes without warning signs, blurred boards, cycle-only signs and moving pedestrians or vehicles. Label them as negatives only after confirming all supported objects in that image are annotated. Keep a reviewed list of ambiguous examples and the reason for exclusion or reassignment. Evaluate each category and each condition tag on held-out real scenes; artwork or synthetic fixtures do not establish real-world accuracy.

Speech may state the accepted sign meaning and quote accepted OCR text. It must not turn an arrow, distance, destination, warning or unreadable text into a navigation instruction or crossing decision. If text is not accepted, omit that text rather than inventing a replacement.

## Manifest and validation

The offline `manifest.json` has `datasetId`, `version`, `catalogueSha256`, `seed: 42`, `sources` and `scenes`. Each source records `sourceId`, a human-reviewed `permission` record and `usageTerms`. Each scene records `sceneId`, `sourceId`, `locationGroup`, `split`, `mediaKind` (`image` or `video-frame`), a relative `mediaPath`, its lowercase SHA-256, original `width`/`height`, `conditionTags`, and `annotations`. Each annotation records `classId`, the matching catalogue `category`, an upright-pixel `box` in `[x, y, width, height]` form, `transcript` (text or `null`) and `trackId` (text or `null`; required for video frames). An empty annotations list marks a negative scene.

Run `PYTHONPATH=training training/.venv/bin/python -m road_sign_training.validate_dataset --manifest <path>` before training. `assign_grouped_splits` gives a reproducible seed-42 assignment with 70/15/15 scene-count targets. All scenes connected by a shared source or location stay in one split; impossible exact ratios never justify splitting a group. The validator checks all eight classes in each split and requires negatives. If these checks fail, collect or annotate more authorised scenes. Apply any augmentation only to the training split after validation; validation and test media stay unchanged.
