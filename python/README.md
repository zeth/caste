# caste - T-Shirt sizes for hardware

Opinionated hardware classification library for Python.

`caste` is a deliberately simple library that inspects a user’s machine and assigns it to a small, practical "hardware caste" (like Mini, User, or Developer). This helps you set sensible defaults for your application based on the user's hardware without manually parsing RAM, CPU, and GPU stats.

## Installation

```bash
pip install caste
```

## Quick Start

```python
import caste

# Get a simple one-word label for the machine
print(caste.detect_caste_word())  # e.g., "User"

# Get the label and the reason for the classification
label, reason = caste.detect_caste()
print(f"Caste: {label} (Reason: {reason})")

# Get raw hardware facts if you want to perform your own logic
facts = caste.detect_hw_facts()
print(f"RAM: {facts['ram_bytes'] / 1024**3:.1f} GB")
print(f"Discrete GPU: {facts['has_discrete_gpu']}")
```

## Hardware Castes

- **Mini** — Microcomputers, embedded systems, classic/legacy PCs.
- **User** — Standard consumer-level machines.
- **Developer** — High-end personal computers.
- **Workstation** — Professional content-creation or high-end gaming machines.
- **Rig** — Dedicated servers or specialised compute systems.

## API Reference

`detect_hw_facts()` returns a dictionary with:

- `ram_bytes` (int)
- `physical_cores` (int)
- `logical_threads` (int)
- `gpu_kind` (0=None, 1=Integrated, 2=Unified, 3=Discrete)
- `vram_bytes` (int)
- `has_discrete_gpu` (bool)
- `is_apple_silicon` (bool)
- `is_intel_arc` (bool)

## Why use caste?

Modern applications often launch with no idea of the underlying hardware capability. If defaults are tuned only for a developer’s machine, it can lead to sluggish experiences on low-end hardware or not showing off your apps's best capabilities on high-end systems. `caste` provides a standardized way to bridge this gap.

There is also a C++ library and an executable for shell scripts and build systems. See the Github repo for details.

## License

MIT
