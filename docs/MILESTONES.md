# KAMI Independent Development Gates

1. Original Windows application shell, native graphics UI, responsive light/dark theme and accessibility.
2. Independently implement Second Life authentication, TLS identity and session management.
3. Independently implement region and capability protocols, object updates and scene state.
4. Create original **DirectX 12-only** world renderer for geometry, materials, terrain, avatars and animations. Establish native D3D12 device/graphics queue first, then swap chain, resource heaps, shaders, PBR, and scene integration with measured frame times. Do not implement an OpenGL or D3D11 fallback. Direct2D/DirectWrite is allowed solely for the desktop UI.
5. Implement interoperable inventory and outfit synchronization with server permissions.
6. Implement real IM and social presence, opt-in negotiated receipts, and privacy controls.
7. Implement creator workflow and server-valid mesh uploads, previews and LOD.
8. Verify secure releases, signed updates, rollback and Windows accessibility/performance.

No feature may be represented as working until its subsystem passes end-to-end testing. The original project is not a reskin or code fork of a viewer. The old experimental branch is unrelated and remains unchanged during this migration.
