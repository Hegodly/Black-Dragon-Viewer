# Live current-scene graphics preview

Adds a session-only preview switch at the top of Preferences > Display. Existing
native renderer-bound controls change the actual current scene immediately.
Preview mode fades Preferences, including text and controls, using a scoped UI
draw context, revealing the world behind it. Opacity is adjustable from 25–100%.
End preview restores normal opacity without changing graphics settings. Opening
or closing Preferences resets preview mode. Existing Apply/OK/Cancel semantics
remain unchanged. This is not an A/B comparison or a separate viewport thumbnail.

No second scene render, screenshot readback, animated wallpaper, background blur
or new graphics backend is introduced. Experimental settings retain their existing
limitations. Some graphics changes reallocate resources and can briefly hitch.
The chrome/fire-garden UI designs remain separate concept images.

Verified: source integration, LLSD/XUI parse, original graphics binding preservation,
control positioning, and open/close reset hooks. Windows build and in-world visual,
focus, text readability, Apply/Cancel checks remain pending.
