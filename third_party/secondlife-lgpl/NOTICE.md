# Second Life login identity compatibility module

Upstream: [Linden Lab Second Life viewer](https://github.com/secondlife/viewer)
(`indra/newview/llappviewerwin32.cpp`, method
`LLAppViewerWin32::generateSerialNumber()`, `develop` commit
`b645b767684ccabdfd285f0c2a55855697029909`).

The source `src/sl_reference/official_login_identity.cpp` implements the
Windows login `id0` device-identifier algorithm using Windows BCrypt
rather than the official viewer's LLMD5 class. It is an adaptation of the
Linden Lab code, **not** a verbatim copy of the full viewer transport.

The module is licensed under GNU Lesser General Public License **2.1 or
later**. The full license is included in LICENSE.LGPL-2.1.txt. When
distributing this DLL, supply its LGPL-covered source and comply with
the license's notices/relinking requirements; do not apply KAMI's
proprietary terms to this module. KAMI's separate application source
can retain its own license subject to the LGPL's conditions.

The rest of the official viewer's login, LLCore HTTP, LLSD,
session/capability and simulator networking implementations have
**not yet** been imported; the KAMI prototype does not become a
complete Second Life viewer merely by adding the identity bridge.

## Windows machine fingerprint

The same LGPL DLL now also adapts the current official viewer's
`LLMachineID::init()`, `LLWMIMethods::getGenericSerialNumber()` and
`llHashedUniqueID()` behavior from
`indra/newview/llmachineid.cpp` and
`indra/newview/llhasheduniqueid.cpp` (upstream `develop` commit
`b645b767684ccabdfd285f0c2a55855697029909`).

On Windows, KAMI queries WMI for the motherboard/system-product UUID,
then falls back to the operating-system serial number. It folds the
UTF-16 serial's successive characters into six bytes and hashes these
bytes with MD5, matching the viewer's Windows `mac` parameter format.
Only if WMI is unavailable does KAMI fall back to hashed adapter bytes.
No physical serial or UUID is written to log files.

## Active official LLUDP zero-code DLL (KAMI SL migration)

The `KAMI_SL_LLUdpCodec.dll` shared library now compiles **unchanged**
`indra/llmessage/llzerocode.h` from Linden Lab's official Second Life
viewer, upstream repository `secondlife/viewer`, pinned reference
`5a43248759bd1b2e8d7ebf4ce00fe55c181824ac` (upstream file blob
`7246c9d9466f19d7f0323aa4bfe881eda73e278e`).

The source is vendored under
`third_party/secondlife-lgpl/lludp/llzerocode.h`, with a small local
`stdtypes.h` typedef shim and a bounded Windows C ABI adapter at
`src/sl_reference/official_lludp_zerocode.cpp`. The adapter is also
licensed under LGPL 2.1 ONLY. The only cross-boundary exported functions
are `kami_sl_official_zerocode_encode` and `_decode`, through
`include/kami/protocol/official_lludp_zerocode.h`.

Distribute these source files, their license and notices with the DLL;
allow replacing or relinking the LGPL portion in compliance with the LGPL.
The independent KAMI main program and DX12 rendering implementation remain
separately authored. Downstream distributors should review all license
requirements before commercial/public distribution.

**Scope:** this DLL replaces only the legacy zero-run encode/decode code for
the existing KAMI LLUDP circuit. It is not Linden Lab's full
`LLMessageSystem`, simulator session, LLSD, EventQueue, object or asset
handling. The full official networking backend remains unimplemented.

