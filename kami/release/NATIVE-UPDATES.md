# KAMI Native Release Discovery — safe pre-DX12 implementation

The Windows login now checks KAMI's **official GitHub Releases /latest** API
asynchronously, without blocking typing or login. It only reports newer stable
three-component version tags (for example v5.6.4 after 5.6.3). No release or
network failure is NOT reported as "up to date". No Black Dragon release
service is queried from this login UI.

The **KAMI RELEASES** button opens the project's public releases page.
The notification opens that same fixed HTTPS page when the user chooses Yes;
the API's URL is validated but never used to launch a downloaded executable.
TLS peer and hostname verification are required, response bodies are capped,
and request timeouts avoid indefinite work. This implementation deliberately
DOES NOT auto-download, auto-run or auto-install patches.

The older packaged manual hotfix remains a standalone archive for the one
specifically supported executable hash; the viewer does not automatically
apply that archive. A future automatic updater must implement signed release
metadata and archive verification, rollback and restart-safe replacement,
before enabling unattended installation. Do not claim it's shipped already.

Check the Windows login both online/offline, with no published releases and
with a newer stable release; check API rate limits, input focus, and close
behavior. Also verify that the project's own stable version tags use the
same three-number scheme as LLVersionInfo::getShortVersion().
