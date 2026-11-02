# Smart Sort: Brand Folders

Select a container folder in the inventory tree (for example Objects or Unsorted Purchases), then use Smart Sort > Brand Folders > Use Selected Folder. You can instead choose My Inventory. Review the destination preview and choose Organize Folders to submit the moves.

The scan covers direct child product folders only. Selecting a single product folder scans inside that product; select its parent if you want to group it alongside other purchases. Folder contents and names are preserved. Destination [STORE] groups are created or reused inside the chosen source container.

Recognized explicit examples:

- [Hayabusa] Reed FATPACK UNPACKER -> [HAYABUSA]
- -Pretty Liars- Real Kisser -> [PRETTY LIARS]
- .:LW:. Eli Evo X Bento Mesh Head -> [LW]
- ::LV:: Eli Evo X Bento Mesh Head -> [LV]
- Store - Product -> [STORE]

Names with no explicit brand delimiter, such as The P V3.0 by ASA Studios & Session Skins, remain untouched: this version does not guess creator/store relationships from arbitrary product descriptions. Exact group names such as [LW] are not treated as products.

System folders never move. Source scopes must be the current user's root, ordinary user folders, Objects, Clothing, Body Parts, or ordinary subfolders of these. Trash, outfits, Library and other protected inventory trees are excluded. Every source/destination and current parent is rechecked when applying, including after asynchronous folder creation. Limits remain 500 inspected folders and the existing per-run move/group caps. Loading folders must finish fetching before previewing.

Tests execute the actual C++ brand parser, scope validator, existing destination lookup and move helper against an inventory double. They check recognized/unknown names, existing group reuse, protection boundaries, preservation of children, stale parent changes and repeated runs. An isolated installation test checks the real notification buttons and native callback wiring. Simulator synchronization still needs in-world testing in the Windows build.
