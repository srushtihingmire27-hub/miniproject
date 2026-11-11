#!/bin/sh
#
# publish.sh - Create the CASEFILE repository on GitHub and push the project.
#
# The script is used once, to publish the project to the account
# srushtihingmire27-hub. It needs a Personal Access Token with the "repo"
# scope, supplied through the GITHUB_TOKEN environment variable so that the
# token never appears in the shell history.
#
# Usage:
#   GITHUB_TOKEN=github_pat_xxx ./docs/publish.sh
#
# The token is stored in the macOS keychain by the git credential helper that
# is already configured, so it is not written into the repository or any file.

set -e

REPO_OWNER="srushtihingmire27-hub"
REPO_NAME="CASEFILE"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if [ -z "$GITHUB_TOKEN" ]; then
    echo ""
    echo "  [!] GITHUB_TOKEN is not set."
    echo ""
    echo "      Create a personal access token at:"
    echo "        https://github.com/settings/tokens"
    echo ""
    echo "      Tick the 'repo' scope, then run:"
    echo "        GITHUB_TOKEN=github_pat_xxx ./docs/publish.sh"
    echo ""
    exit 1
fi

echo ""
echo "  Publishing CASEFILE to $REPO_OWNER/$REPO_NAME"
echo ""

# 1. Create the repository if it does not exist yet.
echo "  1. Creating the repository (skipped when it already exists)"
EXISTING=$(curl -s -o /dev/null -w "%{http_code}" \
    "https://api.github.com/repos/$REPO_OWNER/$REPO_NAME" \
    -H "Authorization: Bearer $GITHUB_TOKEN")

if [ "$EXISTING" = "200" ]; then
    echo "     The repository already exists."
else
    curl -s -X POST "https://api.github.com/user/repos" \
        -H "Authorization: Bearer $GITHUB_TOKEN" \
        -H "Accept: application/vnd.github+json" \
        -d "{\"name\":\"$REPO_NAME\",\"description\":\"CASEFILE: Forensic Evidence Management and Crime Investigation System - a modular C mini-project\",\"private\":false,\"has_issues\":true,\"has_wiki\":false,\"has_projects\":false}" \
        -o /tmp/opencode_repo_create.json

    if grep -q '"id"' /tmp/opencode_repo_create.json; then
        echo "     Repository created."
    else
        echo "     [!] The repository could not be created:"
        grep '"message"' /tmp/opencode_repo_create.json | head -2
        exit 1
    fi
fi

# 2. Store the token in the keychain and point the remote at the repository.
echo "  2. Configuring the remote"
cd "$PROJECT_DIR"

printf 'https://%s@github.com/%s/%s.git\n' "$GITHUB_TOKEN" "$REPO_OWNER" "$REPO_NAME" \
    | git remote remove origin 2>/dev/null || true
git remote remove origin 2>/dev/null || true
git remote add origin "https://github.com/$REPO_OWNER/$REPO_NAME.git"

git config credential.helper osxkeychain
printf 'protocol=https\nhost=github.com\nusername=%s\npassword=%s\n\n' \
    "$REPO_OWNER" "$GITHUB_TOKEN" | git credential approve

# 3. Push the history.
echo "  3. Pushing the commit history"
git push -u origin main

# 4. Make the branch the default branch of the repository.
echo "  4. Setting the default branch"
curl -s -X PATCH "https://api.github.com/repos/$REPO_OWNER/$REPO_NAME" \
    -H "Authorization: Bearer $GITHUB_TOKEN" \
    -H "Accept: application/vnd.github+json" \
    -d '{"default_branch":"main"}' \
    -o /dev/null

echo ""
echo "  Published: https://github.com/$REPO_OWNER/$REPO_NAME"
echo ""
