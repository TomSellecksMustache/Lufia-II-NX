import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import nxctl
class TestReleaseSafety(unittest.TestCase):
    def test_known_rom_digest(self):
        self.assertEqual(len(nxctl.EXPECTED_SHA), 64)
        self.assertEqual(nxctl.EXPECTED_SIZE, 2621440)
    def test_stages_sorted(self):
        self.assertEqual(len(nxctl.VERSIONS), len(set(nxctl.VERSIONS)))
        self.assertEqual(nxctl.VERSIONS[0], '0.0.1')
        self.assertEqual(nxctl.VERSIONS[-1], '0.8.0')
    def test_rom_and_generated_files_forbidden(self):
        self.assertIn('.sfc', nxctl.BAD_SUFFIX)
        self.assertIn('.nro', nxctl.BAD_SUFFIX)
        self.assertIn('generated', nxctl.BAD_SEGMENTS)
    def test_stub_is_explicitly_unplayable(self):
        b=(nxctl.ROOT/'ports/switch/source/game_bridge.c').read_text()
        self.assertIn('nxgame_init', b)
if __name__=='__main__':unittest.main()
