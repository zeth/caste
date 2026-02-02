import caste


def test_detect_caste_word():
    word = caste.detect_caste_word()
    assert word in {"Mini", "User", "Developer", "Workstation", "Rig"}


def test_detect_caste_tuple():
    word, reason = caste.detect_caste()
    assert word in {"Mini", "User", "Developer", "Workstation", "Rig"}
    assert isinstance(reason, str)


def test_detect_hw_facts_shape():
    facts = caste.detect_hw_facts()
    assert isinstance(facts, dict)
    assert facts["ram_bytes"] >= 0
    assert isinstance(facts["physical_cores"], int)
    assert isinstance(facts["logical_threads"], int)
    assert facts["gpu_kind"] in {0, 1, 2, 3}
    assert facts["vram_bytes"] >= 0
    assert isinstance(facts["has_discrete_gpu"], bool)
    assert isinstance(facts["is_apple_silicon"], bool)
    assert isinstance(facts["is_intel_arc"], bool)


def test_version_string():
    assert isinstance(caste.__version__, str)
    assert caste.__version__
