# Going public checklist

Use this list before flipping `initializeddd/Lucid` from private to public. Some items can only be
enabled after the repository is public (branch protection, CodeQL, Scorecard, Pages on a free plan).
Distribution listings and launch announcements are follow-up work, not prerequisites for changing
visibility. Finish the content, history, release-artifact and clean-commit checks first; then
complete the public-only security and documentation checks immediately after the visibility change.

> **Status (2026-11-04):** The repository remains private. Previous CI runs passed on macOS, Linux,
> Windows, FreeBSD, and OpenBSD, but the current working tree contains substantial uncommitted
> changes that have not been covered by those runs. Review and validate the exact commit intended
> for publication. `docs/assets/social-preview.png` is prepared but still needs to be uploaded.

## 1. Repository settings

- [x] Set the repository description:
      `Lucid (Beta) — a minimal, GPU-accelerated code editor built on Vulkan.`
- [x] Set the homepage to `https://initializeddd.github.io/Lucid/` (or your custom domain).
- [ ] Upload `docs/assets/social-preview.png` under **Settings → General → Social preview**.
      The 1280×640 image features a render of Lucid's ocean lighthouse scene and uses no purple
      gradients. Safari was signed out of GitHub when checked on 2026-11-04, so the private
      repository settings were inaccessible.
      GitHub's repository metadata still reports the owner avatar as its open-graph image.
- [x] Review topics: `code-editor`, `cpp`, `cross-platform`, `glfw`, `gpu`, `moltenvk`, `vulkan`,
      `developer-tools`, `text-editor`, `keyboard-first`, `native`, `macos`, `linux`, `windows`.
- [x] Keep **Issues** and **Discussions** enabled. Decide whether to enable the **Wiki** (docs are
      the single source today).
- [x] Enable **Projects** if you want a public roadmap board.
- [x] Confirm **Allow forking** is enabled for a public project.
      Forking is enabled by default; GitHub rejects the explicit flag for user-owned repositories.
- [x] Review release classifications: all published releases are now marked as pre-releases,
      consistent with their beta names. GitHub's latest-stable-release API returns 404 until a
      stable release is published; use the [releases page](https://github.com/initializeddd/Lucid/releases)
      or a versioned release link for beta downloads. No in-repository reference to the latest
      release API was found when checked on 2026-11-04.

## 2. Security and access

- [ ] Enable **Private vulnerability reporting** (Settings → Code security). The GitHub API
      returned 404 while the repository was private on 2026-11-04; retry after publication.
- [x] Enable **Dependabot alerts** and **Dependabot security updates**.
- [ ] Enable **Secret scanning** and **Push protection**.
- [ ] Enable **CodeQL** default setup or keep the committed `codeql.yml` workflow.
      The workflow is committed but skipped while the repository is private.
- [ ] After the repository is public, configure a **branch protection rule** or **ruleset** for
      `main`, using the actual CI job names as required checks and blocking force pushes and
      deletions. Choose review and bypass rules that the maintainer can satisfy. Do not require
      the current Docs workflow on pull requests: it runs only on pushes and manual dispatch, so
      it would leave pull requests waiting for a check that never starts.
- [ ] Consider requiring signed commits and a signed-off-by trailer.
- [x] Review **Environments**, **Deploy keys**, **Webhooks**, **GitHub Apps** and **Actions secrets**
      for anything that should not be exposed.
- [ ] Confirm the `Scorecard` workflow starts publishing results after the repository is public.

## 3. Documentation site

- [ ] Make the repository public first (Pages on a free plan requires a public repository).
- [ ] Set **Settings → Pages → Build and deployment → Source** to **GitHub Actions**.
- [ ] Run the `Docs` workflow or push a change under `docs/` to deploy.
- [ ] Verify `https://initializeddd.github.io/Lucid/` loads and the screenshots render.
- [x] Update `site_url` in `mkdocs.yml` if you use a custom domain.
      Already set to `https://initializeddd.github.io/Lucid/`; no custom domain is used.
- [x] If you use a custom domain, add `docs/CNAME` and configure the DNS records.
      Not applicable: no custom domain.
- [x] Check that the README and `docs/` links do not point at private-only pages.

## 4. Content cleanup

- [x] Search for private-only wording:
      `rg -n "private repository|requires GitHub access|current private" README.md SECURITY.md docs .github`
- [x] Review published GitHub release notes for obsolete privacy wording and duplicate text.
      Notes were updated on 2026-11-04 without changing release tags or assets. The older beta
      releases were subsequently marked as pre-releases.
- [x] Add two distinct animated views using the ocean lighthouse background to the README and
      documentation. The welcome and editor views were captured from Lucid on 2026-11-04; each
      uses eight sampled frames rather than a continuous screen recording.
- [x] Update `CHANGELOG.md` with the release that will be public.
- [ ] Confirm third-party notices are complete for each release artifact. A source-tree index is
      available in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md); verify the packaged copies
      and platform-specific dependencies separately. The `v0.2.1-beta.2` Linux archive contains
      the Lucid, JetBrains Mono, curl, llama.cpp and PicoSHA2 notices, plus bundled GLFW, Vulkan,
      OpenSSL and X11 libraries. Review the applicable notices for those bundled libraries and
      the other archive contents; the source-tree index alone does not establish package coverage.
      The macOS and Windows ZIPs contain the font and several dependency notices, but no standalone
      Lucid `LICENSE` file was found in either ZIP. The macOS DMG likewise contains the font and
      curl, llama.cpp and PicoSHA2 notices but no standalone Lucid `LICENSE`; it bundles GLFW,
      MoltenVK and the Vulkan loader. The Windows MSI database lists the font and those three
      dependency notices, but no standalone Lucid `LICENSE` filename was found in its string
      table; this is a database inspection, not an installed-file test. Address distribution-notice
      gaps in the release process before announcing the release.
- [ ] Confirm generated background assets are redistributable. The scene scripts generate geometry
      and contain no external download or texture references, but several mention visual references;
      confirm their provenance and rights rather than assuming the scripts prove ownership.
- [ ] Remove internal notes, TODOs and experiments that should not be public.
- [ ] Review every remote branch and tag, not only `main`: public visibility exposes their history
      too. The repository currently has module-publishing and release/CI-validation branches in
      addition to `main`; preserve or remove them deliberately before publication.
- [ ] Check the exact history to be published for secrets with `gitleaks git --redact`.
      On 2026-11-04 a scan of the history reachable from `63ac321` found no leaks. A scan of all
      local refs reported six matches: two private-key header strings used as scanner patterns
      in `src/modules/SecretScan.cpp`, and four SQLite source identifiers. These reviewed matches
      are not credentials. Repeat the scan for the final release commit and review any new findings.
- [ ] Scan the final tracked tree as well as its history. A working-directory scan on 2026-11-04
      reported scanner header patterns, SQLite API identifiers and test credentials from ignored
      downloaded build dependencies; review newly added findings before publishing. Ignored build
      directories are not part of the repository, but untracked files need review before staging.
- [x] Check repository size: `git-sizer --verbose` and `find . -type f -size +5M`.
- [ ] Rotate any credential that ever appeared in the history to be published. Public history is
      permanent; investigate findings before deciding whether rotation or history cleanup is needed.

## 5. CI and releases

- [ ] Confirm `CI` passes on macOS, Linux and Windows for the commit to be published.
- [ ] Confirm `BSD` passes for the commit to be published.
- [ ] Confirm `Docs` builds with `mkdocs build --strict` for the commit to be published.
      On 2026-11-04, the documentation-only review commit `8dbab4e` built locally on macOS and
      passed 42 tests and the strict docs build. Its GitHub CI and BSD jobs did not start because
      GitHub reported a failed payment or spending-limit issue. Repeat checks for the final commit.
- [ ] Confirm `CodeQL` and `Scorecard` are green after the repository is public.
- [x] Confirm the release workflow produces the expected artifacts and SHA-256 checksums.
      The workflow now creates `SHA256SUMS.txt`; older beta releases predate that change.
- [ ] Decide whether to add code signing/notarization for macOS and Windows.
      The `v0.2.1-beta.2` macOS app's existing signature verified on disk on 2026-11-04; this
      does not establish distribution signing or notarization.
- [ ] Consider adding build provenance (`actions/attest-build-provenance`) and an SBOM.
- [ ] Tag the first public release and write public release notes.

## 6. Distribution and discoverability

- [ ] Publish a Homebrew cask or tap for macOS.
- [ ] Publish a winget manifest for Windows.
- [ ] Publish an AUR package and/or Flathub listing for Linux.
- [ ] Verify the install script downloads public release assets.
      All nine files listed in the `v0.2.1-beta.2` checksum manifest matched after an authenticated
      download on 2026-11-04; the anonymous public path remains untested.
- [ ] Submit to relevant `awesome-*` lists and package directories.
- [ ] Prepare a launch post for Hacker News, Product Hunt, Reddit and your own channels.
- [ ] Add a star-history chart and a contributors image after the repository is public:
      - `https://api.star-history.com/svg?repos=initializeddd/Lucid&type=Date`
      - `https://contrib.rocks/image?repo=initializeddd/Lucid`
- [ ] Add a `FUNDING.yml` entry if you want a Sponsor button.

## 7. Community

- [ ] Seed **good first issue** and **help wanted** issues.
- [x] Confirm Discussions categories include Announcements, Q&A, Ideas and Show and tell.
      All four already exist, alongside General and Polls.
- [x] Document the chosen conduct-reporting route: GitHub's reporting tools. GitHub Support reports
      are handled by GitHub and may not be forwarded to Lucid's maintainers.
- [x] Confirm `CODEOWNERS` points at the repository owner, `@initializeddd`.
- [ ] Confirm `SUPPORT.md`, `SECURITY.md` and issue forms are accurate.
- [x] Add a [moderation plan](MODERATION.md) for issues and Discussions.

## 8. Final pre-flight

Run these from a clean checkout of the exact commit intended for publication. The local working
tree contains uncommitted feature work; review it before deciding whether to include it in the
public launch. A history scan must also cover the exact history being published.

```sh
git status
git log --oneline -10
git ls-files | wc -l
gitleaks git --redact --log-opts='--all' .
git-sizer --verbose
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
pip install -r docs/requirements.txt
mkdocs build --strict
```

Once the pre-public checks are complete and the owner chooses to publish, make the repository public:

```sh
gh repo edit initializeddd/Lucid --visibility public --accept-visibility-change-consequences
```

Then enable Pages, branch protection, CodeQL and Scorecard, and verify the docs URL and social
preview.
