# KAMI Viewer — GitHub contributor-license workflow scope

The repository is a third-party fork of the Black Dragon and Second Life viewer sources.

## Why the inherited Check CLA job was failing

The original Second Life workflow uses a private `secondlife/cla-signatures` repository and a secret `SHARED_CLA_TOKEN` owned by Linden Lab. The token is not available to independent forks. Attempts to run that action in `Hegodly/Black-Dragon-Viewer` fail before any contributor can sign a form. This is a CI configuration error, not a Windows compilation or GPU-rendering error.

The corrected default-branch workflow only launches the upstream token-dependent action when `github.repository == 'secondlife/viewer'`. GitHub executes `pull_request_target` workflows from the default branch; applying this guard exclusively to KAMI feature branches is not sufficient.

## What this does not mean

- The fix does **not** grant KAMI proprietary rights to upstream code, logos, soundtracks, fonts or third-party dependencies.
- It does **not** waive LGPL 2.1 source-redistribution and notice obligations, or file-specific licenses.
- It does **not** imply Linden Lab approved KAMI as a third-party viewer.
- If this independent project later chooses to require its own contributor agreement, that must be configured for this repository with appropriate authorized credentials and a distinct agreement. Do not reuse Linden Lab's private signature store.

The KAMI source licensing and attribution notice is documented in `KAMI-LICENSING.txt` on the integration branch.

## Test status

A successful GitHub Actions Windows compile is evidence of build-system compatibility, not a substitute for real installed viewer testing, live GPU framebuffers, Second Life login, asset loading, or performance measurements.