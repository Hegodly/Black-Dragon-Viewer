#!/usr/bin/env python3
"""Validate Kami Follow Avatar wiring without a running Second Life region.

This is a source-contract/preflight check, not a substitute for building and
testing in a populated simulator.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

agent = source("indra/newview/llagent.cpp")
agent_h = source("indra/newview/llagent.h")
avatar = source("indra/newview/llavataractions.cpp")
avatar_h = source("indra/newview/llavataractions.h")
people = source("indra/newview/llpanelpeoplemenus.cpp")
viewer_menu_cpp = source("indra/newview/llviewermenu.cpp")

def check(test, description):
    if not test:
        raise AssertionError(description)
    print("OK:", description)

for path in (
    "indra/newview/skins/default/xui/en/menu_people_nearby.xml",
    "indra/newview/skins/default/xui/en/menu_viewer.xml",
    "indra/newview/skins/default/xui/en/notifications.xml",
):
    ET.parse(ROOT / path)
    print("OK: XML parses", path)

check("void LLAgent::startFollowPilot" in agent and "void LLAgent::autoPilot" in agent,
      "uses existing native autopilot")
check("mAutoPilotLastFollowProgressPos" in agent_h and
      "dist_vec(getPositionGlobal(), mAutoPilotLastFollowProgressPos)" in agent,
      "follower movement measured independently of leader distance")
check("target_dist <= mAutoPilotStopDistance" in agent and
      "setControlFlags(AGENT_CONTROL_STOP);" in agent,
      "does not exit follow when within stop distance")
check("!object || object->isDead()" in agent and
      "dist_vec(getPositionGlobal(), object->getPositionGlobal()) > 128.0" in agent,
      "stops if avatar is not visible or leaves rendered range")
for command in ("moveAt", "moveAtNudge", "moveLeft", "moveLeftNudge", "moveUp", "moveYaw"):
    start = agent.index("void LLAgent::" + command + "(")
    end = agent.find("\n//-----------------------------------------------------------------------------", start + 1)
    body = agent[start:end if end != -1 else None]
    check("stopAutoPilot(true)" in body, command + " cancels follow on manual input")

for name in ("canFollowAvatar", "followAvatar", "isFollowingAvatar", "stopFollowingAvatar"):
    check(name in avatar and name in avatar_h, "avatar actions expose " + name)
check("gAgent.startFollowPilot(id, gAgent.getFlying(), 3.0f)" in avatar,
      "3m following uses normal movement without forced flight")
check('registrar.add("Avatar.FollowNearby"' in people and
      'registrar.add("Avatar.StopFollowingNearby"' in people,
      "nearby menu callbacks registered")
check('can_follow_avatar' in people and 'can_stop_following' in people,
      "nearby menu enable rules")
check('commit.add("World.KamiStopFollowing"' in viewer_menu_cpp and
      'enable.add("World.KamiCanStopFollowing"' in viewer_menu_cpp,
      "global stop action registered")
menu_people = ET.parse(ROOT / "indra/newview/skins/default/xui/en/menu_people_nearby.xml").getroot()
world_menu = ET.parse(ROOT / "indra/newview/skins/default/xui/en/menu_viewer.xml").getroot()
notifications = ET.parse(ROOT / "indra/newview/skins/default/xui/en/notifications.xml").getroot()
for name in ("follow_avatar", "stop_following"):
    check(menu_people.find(".//menu_item_call[@name='" + name + "']") is not None,
          "nearby menu contains " + name)
check(world_menu.find(".//menu_item_call[@name='Kami Stop Following']") is not None,
      "Stop Following available in World menu even when leader is no longer selected")
for name in ("KamiFollowAvatarStarted", "KamiFollowAvatarStopped", "KamiFollowAvatarNotPossible"):
    check(notifications.find(".//notification[@name='" + name + "']") is not None,
          "notification exists: " + name)
print("Kami Follow Avatar preflight: PASS")
