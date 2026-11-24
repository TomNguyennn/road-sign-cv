# Model Bundle Contract v1

The backend requires a verified custom bundle at `MODEL_BUNDLE_PATH` (default `/models/signs-v1`).
There is no generic-model fallback. Missing/incompatible artifacts keep `/api/v1/ready` unavailable
with a safe model-configuration error; camera capture cannot start before readiness.

## Bundle contents

- `detector.onnx`: custom YOLO11n, FP32, batch 1, static 640x640, opset 17, nms=false.
- `manifest.json`: schemaVersion=1, modelId, detectorSha256, trainingRunId, datasetVersion and split
  hashes, exporter version, tested ORT version, input/output names and shapes, preprocessing,
  postprocessing, class order, catalogue hash, licence/provenance, parity evidence reference.
- `catalogue.json`: eight ordered entries with category, spokenName, isTextRelevant, priority,
  included/excluded variants and provenance references. Class membership is not inferred at runtime.
- `policy.json`: policyVersion and all starting thresholds/bounds in research.md. Upper safety
  limits for media/resources cannot be raised solely by supplying a different model bundle.
- `parity.json`: fixture hashes, Python and C++ versions, tolerance settings and observed maximum
  errors. Fixtures must be authorised and separate from held-out evaluation scores.

Tesseract English data and eSpeak voice data are separately checksum-pinned runtime resources;
their versions/hashes are included in the deployment manifest. They are not trained by the YOLO job.

## Class mapping

| ID | Category | Spoken name | Read text | Priority |
|----|----------|-------------|-----------|----------|
| 0 | pedestrian-crossing-warning | Pedestrian crossing warning | false | 0 |
| 1 | children-warning | Children warning | false | 0 |
| 2 | roadworks-warning | Roadworks warning | false | 0 |
| 3 | no-pedestrians | No pedestrians | false | 0 |
| 4 | street-name | Street name | true | 2 |
| 5 | pedestrian-direction | Pedestrian direction sign | true | 1 |
| 6 | bus-stop | Bus stop | true | 2 |
| 7 | temporary-pedestrian-notice | Temporary pedestrian notice | true | 0 |

Associated textual plates are labelled separately when they fit a supported text category;
the first four symbol categories do not promise unrestricted OCR outside their boxes. Directional
text is quoted; do not infer arrows, distance or route instructions unless explicitly present as
recognised text. Catalogue guidelines must distinguish a temporary notice from a normal direction
sign and document ambiguous/excluded examples before annotation.

## Tensor and geometry contract

Input named `images`: float32 `[1,3,640,640]`, RGB channel-first, values divided by 255. Decode to
upright original image first. Letterbox with linear interpolation, scale=min(640/w,640/h), resized
width/height rounded to nearest integer; pad remaining pixels equally, odd extra pixel on
right/bottom, RGB fill 114. Disable automatic stride-sized output and stretching. Use the same
explicit preprocessing in the Python parity harness and C++ code, not independent convenience
predictors with hidden defaults. Preserve scale and padding for coordinate restoration.

Expected exported output named `output0`: float32 `[1,12,8400]` for eight classes; four center-x,
center-y,width,height values in letterboxed input pixels, followed by eight class scores. There is
no separate objectness channel and no extra sigmoid. Export validation must check this actual
shape/semantics; do not silently adapt an incompatible tensor layout.

Take the highest class score per candidate, keep >=0.10 for tracking, perform per-class NMS at
IoU 0.45, then undo padding/scale and clip to original dimensions. Use stable index order on tied
scores. Reject nonfinite coordinates and nonpositive width/height. Allow negative top-left coordinates
before clipping (partially visible signs); reject boxes empty after clipping. Bound candidates sent to tracker to 100 highest
scores. Class/category acceptance uses research.md temporal policies, not the low tracking cutoff.

Validate ONNX opset/IR compatibility with the locked ORT build, input/output name/rank/dtype and
class count at startup. A checksum mismatch is an error, not a reason to download new weights.
Models are read-only and selected at startup; hot reload is outside v1.

## Training and export acceptance

Planned Python command surface (module names use snake_case):

- `python -m road_sign_training.validate_dataset --manifest <path>`: reject missing permissions,
  invalid boxes/transcripts, missing category coverage and source/location leakage between splits.
- `python -m road_sign_training.train --manifest <path> --model yolo11n.pt --seed 42 --output <dir>`:
  fine-tune all eight categories with recorded config; use validation data for tuning only.
- `python -m road_sign_training.export --checkpoint <path> --output <bundle>`: explicit model,
  shape/opset/NMS settings and immutable metadata; no implicit checkpoint/download substitution.
- `python -m road_sign_training.check_parity --bundle <bundle> --fixtures <manifest> --native <binary>`:
  run Python ONNX and C++ inference against identical preprocessed pixels; also compare trained
  framework output to the export, not merely two consumers of the same faulty export.

Parity requires raw tensors within atol=1e-4/rtol=1e-3, identical retained class labels on fixtures,
and restored box coordinates within 1 original pixel away from threshold-boundary cases. Include
non-square images, padding, small signs and multiple categories. Threshold-boundary fixtures need
explicit expected behavior and may not be silently excluded. Failed parity blocks bundle acceptance;
fix export/preprocessing before changing tolerances. These are numerical equivalence checks, not
claims of detector accuracy.

Training, export, and benchmark artifacts must retain dependency/model provenance. No ordinary demo
upload can be used by these commands unless separately collected and authorised as research data.

For Azure delivery, follow [model-promotion.md](model-promotion.md): package authorised parity
fixtures and saved trained-framework reference tensors with the local evidence, upload through
interactive identity to ACR, and select the immutable candidate through reviewed configuration.
The trusted publisher reruns export/native checks against those references before producing the
deployment-only bundle. The runtime bundle contract above is unchanged; fixtures are not deployed.
