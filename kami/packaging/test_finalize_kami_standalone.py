#!/usr/bin/env python3
"""Synthetic portable artifact tests; no Windows runtime implied."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("finalize-kami-standalone.py")
spec=importlib.util.spec_from_file_location("kami_standalone_finalizer", SCRIPT)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class PortableTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        for file in module.REQUIRED:
            path=self.root/file
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text("present",encoding="utf-8")
        (self.root / "KAMIViewer.exe").write_bytes(b"MZ\x01\x02\x03")
        settings=self.root/"app_settings/settings_blackdragon.xml"
        settings.write_text(
          "<llsd><map><!-- Black Dragon Debugs -->"
          "<key>Taskbar</key><string>Allow the taskbar icon of Black Dragon to flash</string>"
          "</map></llsd>", encoding="utf-8")
        (self.root/"app_settings/settings.xml").write_text("<llsd><map/></llsd>",encoding="utf-8")
        (self.root/"skins/default/xui/en/panel_login.xml").write_text(
          "<panel><icon name='kami_fire_garden' image_name='KAMI_FireGarden'/>"
          "<text name='kami_login_title'>K A M I</text>"
          "<button name='kami_login_music_browse'/></panel>",encoding="utf-8")
        (self.root/"skins/default/xui/en/menu_viewer.xml").write_text(
          "<menu_bar><menu name='Me' label='KAMI'/>"
          "<menu name='KAMI_Workspaces' label='KAMI WORKSPACES'/></menu_bar>",
          encoding="utf-8")
        (self.root/"skins/default/xui/en/floater_preferences.xml").write_text(
          "<floater title='KAMI CONTROL DECK'/>",encoding="utf-8")
        buttons="".join(f"<button name='action_{i}'/>" for i in range(12))
        (self.root/"skins/default/xui/en/floater_kami_workspaces.xml").write_text(
          "<floater name='kami_workspaces'>"+buttons+"</floater>",encoding="utf-8")

    def test_clean_only_root_legacy_launcher(self):
        (self.root/"blackdragon-bin.exe").write_bytes(b"old")
        (self.root/"BlackDragonViewer.exe").write_bytes(b"old")
        (self.root/"blackdragon-bin.pdb").write_bytes(b"debug")
        original=(self.root/"app_settings/settings_blackdragon.xml").read_bytes()
        removed=module.finalize(self.root)
        self.assertEqual(set(removed),{"blackdragon-bin.exe","BlackDragonViewer.exe","blackdragon-bin.pdb"})
        self.assertTrue((self.root/"KAMIViewer.exe").exists())
        self.assertTrue((self.root/"app_settings/settings_blackdragon.xml").exists())
        self.assertNotEqual(original,(self.root/"app_settings/settings_blackdragon.xml").read_bytes())
        self.assertNotIn("Black Dragon", (self.root/"app_settings/settings_blackdragon.xml").read_text())
        self.assertIn("NiranV Dean",(self.root/"KAMI-CREDITS.txt").read_text())
        self.assertIn("licenses.txt",(self.root/"KAMI-README.txt").read_text())
        self.assertFalse((self.root/"Start-KAMI.cmd").exists())
        self.assertFalse((self.root/"Install-KAMI-Login-Music.cmd").exists())
        self.assertTrue((self.root/"KAMIViewer.exe").exists())

    def test_dont_delete_unknown_app_or_runtime_dependency(self):
        # An unknown old-branded file must STOP release. Blind deletion could
        # remove a required plugin or texture the native viewer references.
        (self.root/"blackdragon-custom-runtime.dll").write_bytes(b"needed")
        with self.assertRaisesRegex(RuntimeError,"Unclassified"):
            module.finalize(self.root)
        self.assertTrue((self.root/"blackdragon-custom-runtime.dll").exists())

    def test_dont_release_any_first_run_setup_scripts(self):
        for name in ("Install-GShade-Assets.ps1", "Start-KAMI.cmd",
                     "Install-KAMI-Login-Music.cmd"):
            path = self.root / name
            path.write_text("@echo off", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "scripts found"):
                module.finalize(self.root)
            self.assertTrue(path.exists())
            path.unlink()

    def test_dont_release_incomplete_package(self):
        (self.root/"licenses.txt").unlink()
        with self.assertRaisesRegex(RuntimeError,"license"):
            module.finalize(self.root)

    def test_dont_release_obsolete_top_menu_brand(self):
        (self.root/"skins/default/xui/en/menu_viewer.xml").write_text(
          "<menu_bar><menu name='Me' label='Dragon'/>"
          "<menu name='KAMI_Workspaces'/></menu_bar>",encoding="utf-8")
        with self.assertRaises(AssertionError):
            module.finalize(self.root)

    def test_dont_release_fake_or_missing_native_workspace(self):
        (self.root/"skins/default/xui/en/floater_kami_workspaces.xml").write_text(
          "<floater name='kami_workspaces'><button name='fake'/></floater>",
          encoding="utf-8")
        with self.assertRaises(AssertionError):
            module.finalize(self.root)

if __name__ == "__main__":
    unittest.main()
