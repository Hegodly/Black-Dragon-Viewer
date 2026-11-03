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
