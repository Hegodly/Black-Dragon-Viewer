# KAMI Area Search

Open **KAMI (Me) > World > KAMI Area Search**. Refresh scans root objects currently known to the viewer in the current region. Search filters by object name, description, and owner username or UUID. Results show distance, name, description and owner. Focus Camera looks at the selected object; Show Beacon marks its location. Pause stops new property requests. Refresh starts a new snapshot; turn or move first to discover additional objects.

This is a KAMI adaptation of the Area Search concept reviewed in Firestorm's `indra/newview/fsareasearch.cpp` and `.h` (Techwolf Lupindo / Phoenix Firestorm Project, LGPL-2.1-only). It uses the existing viewer ObjectPropertiesFamily request path rather than importing Firestorm's selection batching, bridge, blacklist, RLVa, and custom list dependencies. It does not implement Firestorm's full advanced actions or filter set.

No simulator privileges or object permissions change. The scan excludes attachments, avatars, child prims and neighboring regions. Maximum 4,096 objects and 16 requests per second; no background idle callback. Closing or pausing stops requests. Changing region clears old results; late replies from another region are ignored. Missing replies remain unreceived until Refresh; there is no retry flood. Group-owned objects may display owner UUIDs. Results and property cache are memory-only.

Validation: `python kami/search/test-area-search.py` exercises the real installer in an isolated fixture. `--integrated` checks actual generated source and XUI in Windows CI. Native compilation and in-world behavior require the Windows build and viewer testing.
