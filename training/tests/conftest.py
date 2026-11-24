from pathlib import Path

import pytest


@pytest.fixture
def isolated_output_dir(tmp_path: Path) -> Path:
    return tmp_path
