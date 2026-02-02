# Caste - Hardware T-shirt sizes

One size does not fit all. But three sizes give workable defaults to most.

For most of human history, clothing was made bespoke. Modern industrial militaries introduced mass-produced clothing sizes, especially during WWII, when the US Army Quartermaster Corps dressed over 16 million servicemen using just three height categories: Short, Regular, and Long. These spread to the civilian world.

Caste is a deliberately simple, opinionated hardware-classification library.
It inspects a user’s machine and assigns it to a small, practical category.

Caste is provided as:

* a C++ library,
* a Python module,
* a standalone executable suitable for shell scripts, build systems, and CI.

## Build Guide

On Linux, BSD and Mac, run:

```bash
./make.sh
cd build
./caste
```

On Windows run:

```bash
make.bat
cd build/Release
caste
```

If you want to know why that machine was placed in that caste, use the --reason argument.

```bash
caste --reason
```

## C++ Library Usage

### Header + API

Include the header:

```cpp
#include "caste.hpp"
```

Most direct usage:

```cpp
auto result = detect_caste();
// result.caste is an enum
// result.reason is a short string (optional)
```

If you only want the single-word label:

```cpp
std::string label = detect_caste_word();
```

If you want raw hardware facts (RAM/CPU/GPU) for your own logic:

```cpp
HwFacts facts = detect_hw_facts();
```

### CMake integration

Option A: add this repo as a subdirectory

```cmake
add_subdirectory(path/to/caste)
target_link_libraries(your_app PRIVATE caste)
```

Option B: use the installed package

```cmake
find_package(caste REQUIRED)
target_link_libraries(your_app PRIVATE caste::caste)
```

If you are installing from source, a standard install works:

```bash
cmake -S . -B build
cmake --build build
cmake --install build
```

## Python bindings

See `python/README.md`.

## Why?

Modern applications increasingly adapt their behaviour based on available hardware.

AI inference, video processing, rendering, simulation, and even UI effects often benefit from different algorithms or defaults at different performance levels. The challenge is choosing sensible defaults before the user touches a settings menu.

Research consistently shows that:

* Users form judgments about software very quickly [^1]
* Most users never change default settings [^2]

That means defaults matter more than documentation.

Yet applications often launch with no idea whether they are running on:

* a microcomputer,
* an aging laptop,
* a mid-range consumer PC,
* or a high-end workstation.

If defaults are tuned only for the developer’s machine, the result is predictable:

* sluggish experiences on low-end hardware,
* wasted capability on high-end systems,
* unnecessary user frustration.

Caste exists to close that gap.

## Inclusive by design

There is a long tail of users running less powerful hardware than developers. [^3]

When applications implicitly assume modern, high-end machines, they unintentionally exclude:

* users in lower-income regions;
* users extending the life of older devices;
* schools, libraries, and shared computers;
* embedded and low-power systems.

Scaling behaviour by hardware capability makes software more inclusive without penalising performance where it is available.

## Environmental motivation

Electronic waste is a growing global problem. [^4]

Extending the usable life of existing hardware — rather than forcing upgrades through poor defaults — is increasingly recognised as one of the most effective ways to reduce environmental impact.

Software that adapts gracefully to older machines helps keep them useful longer.

## Focused codebases

Hardware detection code is:

* low-level
* platform-specific
* verbose
* usually written once and forgotten

Embedding it directly into application logic pollutes codebases for minimal long-term value.

Caste isolates that complexity into a small, reusable library so your application code can stay focused on what it actually does.

### The core castes

These cover the vast majority of use cases:

* **Mini** — microcomputers, embedded systems, classic PCs
* **User** — solid consumer-level machines
* **Developer** — high-end personal computers

### Extended castes

For unusually powerful systems:

* **Workstation** — professional content-creation or high-end gaming machines
* **Rig** — dedicated servers or specialised compute systems (e.g. AI inference/training)

## Tested operating systems

* Ubuntu 24.04
* Windows 11
* macOS 15 (Sequoia)
* FreeBSD 14.3

### Expected to work on

* Windows 7 and later
* Any modern Linux distribution
* macOS 12 (Monterey), 13 (Ventura), 14 (Sonoma)

### Planned or experimental support

* OpenBSD
* NetBSD
* DragonFly BSD
* Haiku
* OpenIndiana
* Older macOS versions (10–11)
* Windows XP

## References

[^1]: **Nielsen Norman Group**.  
      “Users often leave web pages in 10–20 seconds, but pages with a clear value proposition can hold people’s attention for much longer.”  
      https://www.nngroup.com/articles/how-long-do-users-stay-on-web-pages/

[^2]: **Thaler, R. H., & Sunstein, C. R. (2008)**.  
      *Nudge: Improving Decisions About Health, Wealth, and Happiness.*  
      Yale University Press.  
      Supporting UX discussion:  
      https://www.nngroup.com/articles/default-settings/

[^3]: **World Bank**.  
      “The digital divide persists across income groups, regions, and populations, affecting access to and effective use of digital technologies.”  
      https://www.worldbank.org/en/topic/digitaldevelopment/brief/digital-divide  
      **OECD**.  
      “Differences in access to high-quality devices and infrastructure remain a key dimension of the digital divide.”  
      https://www.oecd.org/digital/bridging-the-digital-divide/

[^4]: **European Environment Agency**.  
      “Extending the lifetime of electronic products is one of the most effective ways to reduce their environmental impacts.”  
      https://www.eea.europa.eu/publications/electronics-and-obsolescence  
      **UN Global E-waste Monitor**.  
      https://ewastemonitor.info/
