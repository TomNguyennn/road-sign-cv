# Offline training package

This package is for training, export and evaluation only. It is not a C++ backend
service and must not be installed in the runtime image. Training implementations
and authorised datasets are introduced in later tasks; this setup does not train
or download model weights.

Use Python 3.12. The dependency graph includes PyTorch, torchvision, Ultralytics,
ONNX tooling, OCR error metrics and detection metrics. Python ONNX Runtime is
pinned to 1.22.0 to match the C++ runtime used for later parity checks.

From the repository root, using uv 0.9.10:

```sh
uv venv training/.venv --python 3.12
uv pip sync training/requirements.lock --python training/.venv/bin/python --require-hashes
uv build training --python training/.venv/bin/python --out-dir build/training
uv pip install --python training/.venv/bin/python --no-deps build/training/road_sign_training-0.1.0-py3-none-any.whl
training/.venv/bin/ruff check training/road_sign_training
training/.venv/bin/ruff format --check training/road_sign_training
training/.venv/bin/mypy training/road_sign_training
```

Regenerate the lock deliberately after changing dependency pins:

```sh
uv pip compile training/pyproject.toml --all-extras --universal --python-version 3.12 --generate-hashes --output-file training/requirements.lock
```

The universal lock includes platform markers; successful resolution is not proof
that every platform can install it. Training GPUs, CUDA compatibility and actual
model export/parity require separate validation. Pytest is installed for the later
training tasks; no training behavior tests exist at this setup stage.

## Validation recorded 2026-11-24

On macOS ARM64 with Python 3.12.12: hash-required installation of 72 dependencies,
source distribution/wheel build, local wheel import, Ruff and strict mypy passed.
Imports of PyTorch, torchvision, OpenCV, ONNX, ONNX Runtime, Ultralytics, jiwer,
torchmetrics and scikit-learn passed, as did PyTorch arithmetic and an ONNX Runtime
CPU tensor roundtrip. The Python OpenCV 5.1.0.95 package compiled from its hashed
source archive because no compatible wheel was selected for this host. This is
separate from the C++ OpenCV 4.12.0 installation. Linux/GPU installation and real
training/export/parity have not been tested.
