# GitHub runner on the LAN host (deploys releases)

GitHub's hosted runners cannot reach the LAN, so deploying uses a **self-hosted runner on the production host**. It makes an outbound connection to GitHub only (no inbound port, no VPN). It only ever runs the `deploy` workflow, which downloads a *published release*, verifies its checksums and installs it. Nothing is built on the host.

## Setup (once)
1. In the GitHub repo: **Settings > Actions > Runners > New self-hosted runner** (Linux, x64) and copy the registration token. Note the runner version it shows.
2. On the host: `sudo ./deploy/github-runner/setup-runner.sh johnsie/battlecity-ng <token> <runner-version>` (it prints the archive's SHA-256: compare it with the runner's release notes).
3. **Settings > Environments > New environment** named `lan-production`:
   - *Required reviewers*: add yourself, so a release only reaches production after you click Approve.
   - *Deployment branches and tags*: only tags matching `v*`.
4. **Settings > Actions > General**: set "Fork pull request workflows from outside collaborators" to *Require approval for all outside collaborators*, and keep the repository's default `GITHUB_TOKEN` permissions read-only. Never trigger workflows that use `lan-deploy` from pull requests.
5. `setup-runner.sh` already hands ownership of `/opt/battlecity` and the web root to `gh-runner`; the GitLab runner can no longer deploy (see docs/deploy.md).

## Using it
- `git tag v0.3.0 && git push gitlab master v0.3.0 && git push origin master v0.3.0` builds, tests, packages and publishes the release on GitHub, then waits for your approval and deploys it.
- Re-deploy or roll back: **Actions > deploy > Run workflow**, enter an earlier tag.
- Logs: the workflow run on GitHub, and `journalctl -u actions.runner.*` on the host.

## Security notes
- The runner user cannot do anything as root except restart the `battlecity` service (`/etc/sudoers.d/battlecity-gh-runner`).
- Release assets carry build provenance attestations: `gh attestation verify <file> --repo johnsie/battlecity-ng`.
- GitHub Actions are pinned to major versions and updated by Dependabot pull requests.
