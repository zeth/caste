# Python bindings for caste

## API

The extension exposes three functions and a version string:

```python
import caste

word = caste.detect_caste_word()      # "User"
word, reason = caste.detect_caste()   # ("User", "integrated GPU caste; RAM cap applied")
facts = caste.detect_hw_facts()       # dict of RAM/CPU/GPU facts

print(caste.__version__)
```

`detect_hw_facts()` returns a dictionary with:

* `ram_bytes`
* `physical_cores`
* `logical_threads`
* `gpu_kind` (0=None, 1=Integrated, 2=Unified, 3=Discrete)
* `vram_bytes`
* `has_discrete_gpu`
* `is_apple_silicon`
* `is_intel_arc`

## Install (editable)

```bash
cd python
python -m pip install -e .
```

## Usage

```bash
python -c "import caste; print(caste.detect_caste_word())"
```

## Run Python tests

```bash
cd python
python -m pip install pytest
python -m pytest -q
```
