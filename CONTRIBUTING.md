# Contributing

## Commit messages

This repo generates release notes automatically from commit messages using
[git-cliff](https://git-cliff.org/) (see `cliff.toml`). Write commits in
[Conventional Commits](https://www.conventionalcommits.org/) style so they land
in the right changelog section:

| Prefix | Section |
|--------|---------|
| `feat:` | ✨ Features |
| `fix:` | 🐛 Bug Fixes |
| `docs:` | 📖 Documentation |
| `perf:` | ⚡ Performance |
| `refactor:` | ♻️ Refactor |
| `test:` | ✅ Testing |
| `ci:` / `build:` / `chore:` | 🛠 Maintenance & CI |
| `revert:` | ⏪ Reverts |
| anything else | 🔧 Other Changes |

`Merge` commits and `chore(deps)` / `chore(release)` commits are skipped.

## Linking upstream issues

This is a fork of [profi200/open_agb_firm](https://github.com/profi200/open_agb_firm).
When a change addresses an issue in the upstream tracker, add a footer trailer so
the release notes link back to it:

```
feat: save live settings to config.ini from the menu

Upstream: profi200/open_agb_firm#232
```

Accepted trailer tokens: `Upstream`, `Refs`, `Closes`, `Fixes`, `Resolves`.
The release notes render these as clickable links. Note that upstream issues
live in a different repository, so they are never auto-closed — the link is a
reference only.

To also create a back-reference on the upstream issue's timeline, mention
`profi200/open_agb_firm#<number>` in the pull request description (GitHub only
creates cross-references from commits/PRs/comments, not from release notes).
