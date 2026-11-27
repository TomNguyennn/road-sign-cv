# Dataset readiness for User Story 1

2026-11-25: T027 was blocked because the repository lacked `research-data/uk-signs-v1/manifest.json`, authorised source media, and location, permission or usage-term records. The newer image-folder audit below found unverified local media, but no approved training dataset. The eight-class catalogue and manifest validator do not prove that a suitable UK scene dataset exists.

To complete T027, collect or provide permitted UK photos/video that cover all eight classes plus negatives. Record source permission and usage terms, physical location/source groups, boxes, readable transcripts, video track IDs, condition tags and media SHA-256. Freeze grouped train/validation/test splits, run `validate_dataset`, then review category and condition coverage. Demo-session uploads cannot be silently reused. T028 training, T033 model acceptance and T044 image acceptance remain unverified until this evidence is available.

## Revised catalogue source audit (2026-11-27, T104)

Read-only inventory of `training/road_sign_training/dataset/`: 634 files (14 MiB), all apparent image assets; no annotation, source-permission, licence, location-group, split, or media-checksum manifest was found. The folder is user-provided and remains untouched. The `get_data.py` script calls `load_dataset("Abtinzandi/Obstacle-Detection-Dataset-YOLO")` without pinning a revision; it does not record per-image source or licence, and it does not create a reviewed local subset.

| Revised class | Local folder | Files | Source/rights and inclusion decision |
| --- | --- | ---: | --- |
| 0 pedestrian-crossing-warning | pedestrian-crossing | 83 | Unverified; exclude from training pending per-file provenance and box review. |
| 1 children-warning | children-warnings | 57 | Unverified; exclude. |
| 2 roadworks-warning | roadworks-warnings | 66 | Unverified; exclude. |
| 3 no-pedestrians | no-pedestrian | 98 | Unverified; exclude. |
| 4 street-name | street-names | 72 | Unverified; exclude; text transcripts absent. |
| 5 shared-path-with-cyclist | shared-path-with-cyclist | 68 | Unverified; exclude; distinguish shared path from cycle-only signs. |
| 6 bus-stop | bus-stop-signs | 190 | Unverified; exclude; text transcripts absent. |
| 7 obstacles | obstacles | 0 | No local examples; exclude. |

The [ROD dataset card](https://huggingface.co/datasets/Abtinzandi/Obstacle-Detection-Dataset-YOLO) describes 24,326 images and 25 classes with train/valid/test splits. Potentially relevant source classes are electrical pole (6), dustbin (9), guard rail (13), bench (17), traffic cone (18), traffic barrel (20), plant pot (21), electrical box (22), chair (23), and bicycle rack (24); each needs a reviewed mapping to visible fixed street furniture. People, vehicles, animals, roads and buildings are excluded. The card states that 26 Roboflow collections were merged with in-house Canadian and Iranian imagery, and that the Roboflow inputs retain their original licences. A dataset-level MIT label therefore does not establish rights for every source image. The card does not supply a reviewed per-image source/licence map or source/location grouping needed to rule out split leakage. Its geographic coverage also does not establish UK performance.

ROD's exact immutable revision was not available from the dataset card, and the local script pins none. No ROD media is present under `obstacles/`. Existing ROD split names cannot be adopted as our held-out split until source/location leakage and augmentation history are reviewed. No source above is approved for T027/T105 training or T033 model acceptance. Obtain a pinned revision and per-source rights/permissions, review images and full annotations including classes that would be dropped, establish source/location groups, and collect authorised UK held-out scenes before changing this decision.

## Reviewed subset importer (T105)

`PYTHONPATH=training python -m road_sign_training.import_obstacles --source-root <pinned ROD checkout> --review <review.json> --output <new directory>` accepts only a 40-character immutable dataset commit ID and a per-image review record with source permission/licence, source and location grouping, a declared split, complete-annotation review, and SHA-256 hashes for both image and YOLO label. It copies selected images into a new directory and emits `subset.json`; it refuses an existing output. The source and original label files are read-only.

Accepted source class IDs for candidate class 7 are 6 electrical pole, 9 dustbin, 13 guard rail, 17 bench, 18 traffic cone, 20 traffic barrel, 22 electrical box and 24 bicycle rack. Approved boxes are remapped to target class 7 in `obstacleBoxesYolo`, with original ROD IDs retained in the aligned `sourceObstacleClassIds` list. All other labelled boxes are retained as `ignoredBoxesYolo` and flag `requiresReview`; no such image is considered a confirmed negative or eligible training input by this step. A human must review fixed-versus-moving status, rights, UK relevance and all ignored objects before merging into the eight-class training manifest. The importer has passed synthetic tests only. No pinned ROD checkout or per-image permission review is available locally, so no real subset has been imported or approved.
