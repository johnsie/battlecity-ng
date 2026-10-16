#!/usr/bin/env bash
# One-time setup of the GitHub Actions runner that deploys to this host. Run as root:
#   sudo ./deploy/github-runner/setup-runner.sh <owner/repo> <registration-token> <runner-version>
# Get the token from GitHub: repo > Settings > Actions > Runners > New self-hosted runner (it expires after an hour).
# The runner gets its own unprivileged user, the label `lan-deploy`, and may only restart the game service (sudoers).
set -euo pipefail
REPO=${1:?owner/repo}; TOKEN=${2:?registration token}; VER=${3:?runner version, as shown on the GitHub "New self-hosted runner" page}
USER_NAME=gh-runner
HOME_DIR=/opt/gh-runner
WEBROOT=/var/www/bc.outiva.com

id "$USER_NAME" >/dev/null 2>&1 || useradd --system --create-home --home-dir "$HOME_DIR" --shell /bin/bash "$USER_NAME"
usermod -aG www-data "$USER_NAME"

# The deploy scripts write releases as this user
install -d -o "$USER_NAME" -g "$USER_NAME" -m 0755 /opt/battlecity /opt/battlecity/releases
chown "$USER_NAME:$USER_NAME" /opt/battlecity /opt/battlecity/releases
if [ -d "$WEBROOT" ]; then chown "$USER_NAME:www-data" "$WEBROOT" "$WEBROOT/releases"; fi

# Only the game service may be restarted
SUDOERS=/etc/sudoers.d/battlecity-gh-runner
sed "s/^gitlab-runner /$USER_NAME /; s/GitLab runner user/GitHub runner user/" "$(dirname "$0")/../sudoers-battlecity" > "$SUDOERS"
chmod 0440 "$SUDOERS"; visudo -cf "$SUDOERS"

# Download and verify the runner (the checksum is published with each runner release)
cd "$HOME_DIR"
URL="https://github.com/actions/runner/releases/download/v$VER/actions-runner-linux-x64-$VER.tar.gz"
sudo -u "$USER_NAME" curl -fsSL -o runner.tar.gz "$URL"
echo "Runner archive SHA-256 (compare with the release notes of v$VER at https://github.com/actions/runner/releases):"
sha256sum runner.tar.gz
sudo -u "$USER_NAME" tar -xzf runner.tar.gz && rm runner.tar.gz

sudo -u "$USER_NAME" ./config.sh --unattended --url "https://github.com/$REPO" --token "$TOKEN" \
  --name "$(hostname)-deploy" --labels lan-deploy --work _work --replace
./svc.sh install "$USER_NAME"
./svc.sh start
echo "Runner installed. Next: create the 'lan-production' environment in the repo settings (see deploy/github-runner/README.md)."
