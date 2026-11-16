#!/usr/bin/env bash
# The AvPx pin check. Written 2026-11-14 by Melanikus for AVP_OpenInteng.
#
# What it proves: the editor compiles AvPx's own loader, chunk library and renderer from avpx/, a git submodule. So whatever commit
# avpx/ is pinned to IS the editor's level reader. This check makes sure that commit is one AvPx itself accepted:
#   1. avpx/ points at AvPx's own repository, pinned by commit, and never set to follow a branch on its own;
#   2. the pinned commit is on AvPx's avpx_main (it's avpx_main itself, or behind it);
#   3. every check GitHub ran on that commit finished with success (or was skipped by design), and every check named in
#      tools/avpx-required-checks.txt is among them with success. An empty list fails: the list has to be filled in, never assumed;
#   4. in a pull request, the pin only moves forward (the new commit comes after the old one), unless the pull request's title starts
#      with "Revert pin:" and says why.
# Anything it can't find out (no network, GitHub down, an answer it doesn't understand) FAILS the check. A check that can't run is a
# failed check (Tao 1).
#
# It reads only public facts through GitHub's API (gh api), and changes nothing.
set -euo pipefail

repo="Melanikus/AvP"

fail() {
	echo "::error::$1"
	exit 1
}

# 1. The submodule's own settings.
pin=$(git ls-tree HEAD avpx | awk '$2 == "commit" { print $3 }')
[[ "$pin" =~ ^[0-9a-f]{40}$ ]] || fail "avpx/ isn't a submodule pinned to a commit"

url=$(git config -f .gitmodules --get submodule.avpx.url || true)
[ "$url" = "https://github.com/$repo.git" ] || fail "avpx/ points at '$url', not at https://github.com/$repo.git"

if git config -f .gitmodules --get submodule.avpx.branch >/dev/null; then
	fail "avpx/ is set to follow a branch; it must be pinned by commit only"
fi
echo "The pin: $pin"

# 2. On avpx_main. GitHub's compare answers "behind" when the pin comes before avpx_main's tip, and "identical" when it is the tip.
status=$(gh api "repos/$repo/compare/avpx_main...$pin" --jq .status) || fail "GitHub couldn't compare the pin with avpx_main"
case "$status" in
	behind | identical) echo "It's on avpx_main ($status)." ;;
	*) fail "The pin isn't on avpx_main (GitHub says '$status')" ;;
esac

# 3. Every check on that commit passed, and the required ones are there. "filter=latest" asks for each check's latest run, so a job
# that failed once and was re-run to success counts as its success. But any check that is red now, required or not, blocks the pin
# (a release's antivirus scan, say): someone looks at it first, and then re-runs it to green, or pins another commit.
runs=$(gh api --paginate "repos/$repo/commits/$pin/check-runs?filter=latest&per_page=100" \
	--jq '.check_runs[] | [.name, .status, (.conclusion // "none")] | @tsv') || fail "GitHub couldn't list the checks on the pin"
[ -n "$runs" ] || fail "No checks ran on the pin, so nothing proves it"

bad=$(printf '%s\n' "$runs" | awk -F '\t' '$2 != "completed" || ($3 != "success" && $3 != "skipped")' || true)
if [ -n "$bad" ]; then
	printf '%s\n' "$bad"
	fail "Not every check on the pin passed (listed above: name, state, result)"
fi

required=$(grep -v -E '^[[:space:]]*(#|$)' tools/avpx-required-checks.txt || true)
[ -n "$required" ] || fail "tools/avpx-required-checks.txt names no checks. Fill it in from AvPx's branch protection"
while IFS= read -r name; do
	if ! printf '%s\n' "$runs" | awk -F '\t' -v want="$name" '$1 == want && $3 == "success" { found = 1 } END { exit !found }'; then
		fail "The required check '$name' didn't pass on the pin"
	fi
done <<< "$required"
echo "Every check on the pin passed, the required ones included."

# 4. In a pull request, the pin moves only forward.
if [ -n "${BASE_REF:-}" ]; then
	git rev-parse --verify --quiet "origin/$BASE_REF" >/dev/null || fail "The base branch origin/$BASE_REF isn't there"
	old=$(git ls-tree "origin/$BASE_REF" avpx | awk '$2 == "commit" { print $3 }')
	if [ -n "$old" ] && [ "$old" != "$pin" ]; then
		step=$(gh api "repos/$repo/compare/$old...$pin" --jq .status) || fail "GitHub couldn't compare the old pin with the new one"
		if [ "$step" != "ahead" ]; then
			case "${PR_TITLE:-}" in
				"Revert pin:"*) echo "The pin moves back, and the pull request says it's a revert." ;;
				*) fail "The pin doesn't move forward from $old (GitHub says '$step'). A revert's title starts with 'Revert pin:'" ;;
			esac
		else
			echo "The pin moves forward from $old."
		fi
	fi
fi

echo "The AvPx pin is good."
