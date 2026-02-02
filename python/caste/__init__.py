"""
Opinionated hardware classification library.

Caste inspects a user's machine and assigns it to a practical hardware category
(Mini, User, Developer, Workstation, or Rig) to help set sensible application defaults.
"""

from ._caste import __version__, detect_caste, detect_caste_word, detect_hw_facts

__all__ = [
    "__version__",
    "detect_caste",
    "detect_caste_word",
    "detect_hw_facts",
]
