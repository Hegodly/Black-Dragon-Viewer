# KAMI Messenger — Native Offline Preview

This is an original native Windows / Direct2D + DirectWrite conversation workspace
designed to test layout and local message handling before the independent Second
Life network adapter and authentication service are completed.

## Open the workspace

- **F5** or **Messages** opens the messenger.
- **Design Preview** shows *synthetic messages*, each explicitly marked
  `LOCAL DEMO`, with left-aligned incoming and right-aligned outgoing bubbles.
- Text inside every bubble is **centered horizontally and vertically**, with
  word wrapping. Conversation title and status subtitles are centered.
- **Nearby Chat** and **Groups** intentionally show empty offline states, not
  fabricated recent messages or fake avatars.
- Windows' native edit control allows Unicode input and normal caret/selection.
  Press **Enter** or click **PREVIEW** to add a local-only bubble. The action
  doesn't send, queue or transmit to any other resident.
- Click **ADD DEMO REPLY** to check styling for the other side of the conversation.
- Timestamps shown on local bubbles are local clock times, not network receipts.
- Light/dark mode follows KAMI's existing appearance setting. Rounded
  surfaces, layout spacing and centered typography are drawn with native D2D.
- The in-memory preview and draft are reset on application restart. No
  conversation data is persisted or uploaded, and the demo never asks for a
  Second Life password.

## Read receipts / platform limits

The existing `social::Outbox` can only be acknowledged by future authenticated
transport. The local messenger model NEVER claims Delivered or Read. A genuine
read receipt additionally needs explicit peer support and an authenticated
receipt: ordinary Second Life messages don't guarantee that another viewer can
prove they were opened. The older idea of identifying a peer's viewer is also
gated behind their consent and authenticated metadata.

## Future live integrations

1. Account login, user identity and access permissions.
2. Region chat and IM transport adapters; event queue delivery.
3. Conversation persistence with opt-in history and user-facing controls.
4. Friends, groups, notifications and cross-session identity.
5. Verified peer support for optional delivery/read acknowledgements.

None of these live capabilities are claimed by the offline design preview.
