import codecs
import os
from pathlib import Path
import tempfile
import unittest

import foliage_control as control


class FoliageControlTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'd2r_pcvr.ini'

    def test_new_section_preserves_unrelated_bytes_and_first_backup(self):
        original = b'; settings\r\n[custom]\r\nkeep=12345\r\n[render]\r\nplatform=1\r\n'
        self.path.write_bytes(original)
        control.save_values(self.path, control.CENTERED, control.read_values(self.path))
        self.assertTrue(self.path.read_bytes().startswith(original))
        self.assertEqual(control.read_values(self.path), control.CENTERED)
        control.save_values(self.path, control.DEFAULTS, control.read_values(self.path))
        self.assertEqual(self.path.with_name(self.path.name + '.before-foliage-controls').read_bytes(), original)

    def test_preserve_comments_case_other_sections_and_unknown_keys(self):
        original = '; hi\n[FOLIAGE]\n FOOTPRINT = 1 ; keep this\nunknown=42\n[other]\nspacing=9\n'
        self.path.write_text(original)
        control.save_values(self.path, control.CENTERED, control.read_values(self.path))
        text = self.path.read_text()
        self.assertIn(' FOOTPRINT = 2.00 ; keep this\n', text)
        self.assertIn('unknown=42\n', text)
        self.assertIn('[other]\nspacing=9\n', text)
        self.assertEqual(control.read_values(self.path), control.CENTERED)

    def test_rebase_other_changes_but_reject_stale_foliage_draft(self):
        self.path.write_text('[custom]\nkeep=1\n')
        baseline = control.read_values(self.path)
        self.path.write_text('[custom]\nkeep=2\n')
        control.save_values(self.path, control.CENTERED, baseline)
        self.assertIn('keep=2', self.path.read_text())
        current = self.path.read_bytes()
        with self.assertRaisesRegex(ValueError, 'changed elsewhere'):
            control.save_values(self.path, control.DEFAULTS, baseline)
        self.assertEqual(self.path.read_bytes(), current)

    def test_encodings_and_missing_final_newline(self):
        text = '; café\r\n[foliage]\r\nfootprint=1'
        for encoding in ('utf-8', 'utf-8-sig', 'utf-16', 'latin-1'):
            with self.subTest(encoding=encoding):
                self.path.write_bytes(text.encode(encoding))
                control.save_values(self.path, control.CENTERED, control.read_values(self.path))
                changed = self.path.read_bytes().decode(encoding)
                self.assertTrue(changed.startswith('; café\r\n[foliage]\r\n'))
                self.assertEqual(control.read_values(self.path), control.CENTERED)
        self.path.write_bytes(codecs.BOM_UTF16_BE + text.encode('utf-16-be'))
        control.save_values(self.path, control.CENTERED, control.read_values(self.path))
        self.assertTrue(self.path.read_bytes().startswith(codecs.BOM_UTF16_BE))
        self.assertEqual(control.read_values(self.path), control.CENTERED)

    def test_bad_values_duplicates_and_missing_profiles(self):
        with self.assertRaises(FileNotFoundError):
            control.save_values(self.path, control.DEFAULTS, control.DEFAULTS)
        for text in ('[foliage]\nfootprint=1\nfootprint=2', '[foliage]\n[foliage]\n'):
            self.path.write_text(text)
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                control.read_values(self.path)
        self.path.write_text('[foliage]\nfootprint=nan\nspacing=bad\n')
        self.assertEqual(control.read_values(self.path), control.DEFAULTS)
        before = self.path.read_bytes()
        with self.assertRaises(ValueError):
            control.save_values(self.path, dict(control.DEFAULTS, spacing='nan'), control.DEFAULTS)
        self.assertEqual(self.path.read_bytes(), before)

    @unittest.skipUnless(os.name == 'nt', 'Windows profile cache integration')
    def test_game_profile_api_sees_atomic_replacement(self):
        import ctypes
        api = ctypes.WinDLL('kernel32').GetPrivateProfileStringW
        api.argtypes = (ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p,
                        ctypes.c_wchar_p, ctypes.c_uint, ctypes.c_wchar_p)
        def read():
            buffer = ctypes.create_unicode_buffer(100)
            api('foliage', 'spacing', '1', buffer, 100, str(self.path))
            return float(buffer.value)
        self.path.write_text('[foliage]\nspacing=1\n')
        self.assertEqual(read(), 1)
        control.save_values(self.path, control.CENTERED, control.read_values(self.path))
        self.assertEqual(read(), 2)


if __name__ == '__main__':
    unittest.main()
