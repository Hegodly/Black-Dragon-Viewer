// KAMI RLV Safety - wearer-controlled emergency stop, not an RLV engine.
// Copyright (C) 2026 KAMI contributors; LGPL-2.1-or-later.
// Deliberately fail closed: Black Dragon currently has NO RLVa C++ processor.
// This entry point must remain outside all future RLV restriction code.
#pragma once

#include "llviewercontrol.h"

namespace KAMIRLVSafety
{
inline constexpr bool kNativeRLVaEngineInstalled = false;

inline void setStatus(const char* message)
{
    if (auto* control=gSavedSettings.getControl("KAMIRLVStatus"))
        control->setValue(message);
}

// This entry point is owned by KAMI native UI, not by an object/relay.
// It may not be overridden by a scripted restraint or granted to a script.
// A future RLVa integration MUST clear its real handler queues and live
// restrictions here BEFORE claiming that release succeeded.
inline void emergencyRelease()
{
    if (auto* control=gSavedSettings.getControl("KAMIRLVEmergencyBlock"))
        control->setValue(true);
    if (auto* control=gSavedSettings.getControl("KAMIRLVAllowRelay"))
        control->setValue(false);
    if (auto* control=gSavedSettings.getControl("KAMIRLVAllowTeleport"))
        control->setValue(false);
    if (auto* control=gSavedSettings.getControl("KAMIRLVAllowInventory"))
        control->setValue(false);
    if (auto* control=gSavedSettings.getControl("KAMIRLVAllowCommunication"))
        control->setValue(false);
    if (auto* control=gSavedSettings.getControl("KAMIRLVAllowCamera"))
        control->setValue(false);
    if (auto* control=gSavedSettings.getControl("KAMIRLVAllowEnvironment"))
        control->setValue(false);
    if (auto* control=gSavedSettings.getControl("RestrainedLove"))
        control->setValue(false);
    // As of this build, RLVa is entirely absent. We cannot honestly claim
    // to have cleared active restrictions in an engine that doesn't exist.
    setStatus("RLV BLOCKED. Legacy enable flag disabled. No RLVa engine installed.");
}

// Safety-critical policy boundary for a future vendor-reviewed RLVa port.
// It MUST be invoked by EVERY incoming RLV command, including owner-say,
// remote relays, attachments, and queued/replayed commands. Failing closed
// here prevents a partial port from silently enabling remote control.
inline bool allowInboundCommand()
{
    // NO future developer may flip a single boolean to bypass consent.
    // A separately reviewed per-source/per-command authorization gate,
    // implemented in the actual RLVa dispatch pipeline, is mandatory.
    // Until that exists, absolutely no command is authorized.
    return false;
}
} // namespace KAMIRLVSafety
