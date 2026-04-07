import unittest

import caste


class CasteTests(unittest.TestCase):
    def test_detect_caste_word(self) -> None:
        word = caste.detect_caste_word()
        self.assertIn(word, {"Mini", "User", "Developer", "Workstation", "Rig"})

    def test_detect_caste_tuple(self) -> None:
        word, reason = caste.detect_caste()
        self.assertIn(word, {"Mini", "User", "Developer", "Workstation", "Rig"})
        self.assertIsInstance(reason, str)

    def test_detect_hw_facts_shape(self) -> None:
        facts = caste.detect_hw_facts()
        self.assertIsInstance(facts, dict)
        self.assertGreaterEqual(facts["ram_bytes"], 0)
        self.assertIsInstance(facts["physical_cores"], int)
        self.assertIsInstance(facts["logical_threads"], int)
        self.assertIn(facts["gpu_kind"], {0, 1, 2, 3})
        self.assertGreaterEqual(facts["vram_bytes"], 0)
        self.assertIsInstance(facts["has_discrete_gpu"], bool)
        self.assertIsInstance(facts["is_apple_silicon"], bool)
        self.assertIsInstance(facts["is_intel_arc"], bool)

    def test_version_string(self) -> None:
        self.assertIsInstance(caste.__version__, str)
        self.assertTrue(caste.__version__)


if __name__ == "__main__":
    unittest.main()
