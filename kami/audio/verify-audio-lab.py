#!/usr/bin/env python3
"""Static Audio Lab contract tests for source, UI, settings and Windows startup.

This is a repository CI check, NOT a user-facing installer or feature patch.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
PREFS = ROOT / "indra/newview/skins/default/xui/en/panel_preferences_sound.xml"
SETTINGS = ROOT / "indra/newview/app_settings/settings.xml"
AUDIO = ROOT / "indra/newview/llvieweraudio.cpp"
STARTUP = ROOT / "indra/newview/llstartup.cpp"
FMOD = ROOT / "indra/llaudio/llaudioengine_fmodstudio.cpp"
FMOD_H = ROOT / "indra/llaudio/llaudioengine_fmodstudio.h"

prefs = ET.parse(PREFS)
settings = ET.parse(SETTINGS)
assert prefs.getroot().find(".//*[@name='kami_audio_lab_panel']") is not None, "Missing native Audio Lab panel"
values = {
    "KamiAudioMuteMicrophone": "Boolean",
    "KamiAudioDuckMusicOnVoice": "Boolean",
    "KamiAudioDuckMediaOnVoice": "Boolean",
    "KamiAudioDuckingLevel": "F32",
    "KamiAudioWorldRolloff": "F32",
    "KamiAudioDopplerFactor": "F32",
    "KamiAudioResamplerQuality": "U32",
}
settings_root = settings.getroot()
for key, expected_type in values.items():
    elem = settings_root.find(f".//key[.='{key}']")
    assert elem is not None, f"Missing saved setting {key}"
    # LLSD map uses alternating siblings, so confirm the type via the text source.
    settings_text = SETTINGS.read_text(encoding="utf-8")
    match = re.search(rf"<key>{key}</key>\s*<map>(.*?)</map>", settings_text, re.S)
    assert match and f"<string>{expected_type}</string>" in match.group(1), f"Wrong type for {key}"
    controls = prefs.getroot().findall(f".//*[@control_name='{key}']")
    assert controls, f"No native UI for {key}"

for existing in ("VoiceEchoCancellation", "VoiceAutomaticGainControl", "VoiceNoiseSuppressionLevel"):
    assert prefs.getroot().find(f".//*[@control_name='{existing}']") is not None, existing
assert "initial_value=\"10\"" not in PREFS.read_text(encoding="utf-8").split('name="MediaRollOffRate"')[0][-200:]

audio = AUDIO.read_text(encoding="utf-8")
assert 'getWindow()->getMinimized()' in audio, "Mute-on-minimize must use real minimized state"
assert 'getActive() && mute_when_minimized' not in audio, "Alt-Tab must not cause minimize mute"
assert 'voice_inst->setMicGain(voice_mic_setting());' in audio, "Voice receive mute must not mute mic"
assert 'voice_inst->setMuteMic(kami_mic_mute() || mute_for_minimize);' in audio
assert "getParticipantList(participants)" in audio and "getIsSpeaking(id)" in audio, "Missing remote voice activity"
assert "duck_scan_timer.getElapsedTimeF32()" in audio and "duck_mix +=" in audio, "Missing bounded/scaled ducking"
assert 'kami_duck_media_out() ? sKamiAudioDuckingGain : 1.f' in audio
assert 'gAudiop->setDopplerFactor(doppler);' in audio
assert 'gAudiop->setRolloffFactor(rolloff);' in audio

fmod = FMOD.read_text(encoding="utf-8")
fmod_h = FMOD_H.read_text(encoding="utf-8")
startup = STARTUP.read_text(encoding="utf-8")
assert 'mResamplerQuality(resampler_quality)' in fmod
assert 'FMOD_DSP_RESAMPLER_LINEAR' in fmod
assert 'FMOD_DSP_RESAMPLER_CUBIC' in fmod
assert 'FMOD_DSP_RESAMPLER_SPLINE' in fmod
assert 'U32 resampler_quality = 0' in fmod_h
assert 'gSavedSettings.getU32("KamiAudioResamplerQuality")' in startup
assert 'KamiAudioLab' not in audio or "KamiAudioDuckMusicOnVoice" in audio
print("PASS: Kami Audio Lab native settings, XML, voice capture, mixer and FMOD contracts")
