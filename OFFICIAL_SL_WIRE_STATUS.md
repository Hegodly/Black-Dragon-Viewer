# KAMI — Official SL wire integration stage 1

## What is executable now
The independent Windows DLL `KAMI_SL_Official_Wire.dll` compiles Linden
Lab's exact LGPL 2.1 `LLZeroCode::encode()` and `LLZeroCode::decode()`
implementation from the official Second Life viewer, vendored unchanged
in `third_party/secondlife-lgpl/upstream/llzerocode.h`.

KAMI's actual inbound/outbound LLUDP message packet stream calls that DLL
on zero-coded messages. It replaces hand-written zero run compression in
`src/protocol/lludp.cpp` while preserving:
- KAMI DirectX 12-only rendering, shader cache, UI and FPS controls
- Existing Login, transport, CAPS and mesh paths as a fallback baseline
- Authenticated circuit address, packet numbering, appended ACKs and
  raw high/medium/low frequency message body semantics
- Official behavior: avoid zero coding when encoded payload is not smaller
- Bounds checks around the upstream pointer-based codec; reject malformed
  runs and oversized decompression

The DLL is independently rebuildable/replaceable and ships with LGPL
source and original copyright notice. This is **real first-party source
integration**, not just a planned ABI.

## What is NOT done
- The full `LLMessageSystem` and its `llcommon`, `llmath`,
  `llfilesystem`, `llcorehttp`, `llmessage` dependencies are
  not compiled into KAMI yet.
- Official reliable circuit, message dispatch, object/asset/cache,
  inventory, avatar and EventQueue handlers are not yet connected.
- 598 other ExtraParams records from R43.1 remain unidentified until
  official primitives/object message-processing work is integrated.
- This build does not guarantee that your platform furniture,
  buildings, textures or outfits will load; in-world tests of the
  official wire DLL are still needed.
- Do not call this a complete official Second Life networking backend.

## Security and validation
- Upstream: https://github.com/secondlife/viewer
- Pinned commit:
  https://github.com/secondlife/viewer/commit/5a43248759bd1b2e8d7ebf4ce00fe55c181824ac
- LGPL v2.1 conditions apply to upstream code. Its license and
  replacement sources are included in `LGPL-source`.
- Unit tests exercise zero-code long runs, corrupted frames,
  decompression limits and fallback to an unencoded packet.
- The D3D12 renderer and current custom circuit are unchanged.

## Next integration milestone
Compile and link the actual `LLMessageSystem` behind a separate ABI
library, validate simulator session and typed object decoding, then
remove KAMI's hand-authored protocol parsers once live checks pass.
DO NOT connect two simultaneous simulator circuits.
