# Going public checklist

Use this list before flipping `initializeddd/Lucid` from private to public. Some items can only be
enabled after the repository is public (branch protection, CodeQL, Scorecard, Pages on a free plan).

## 1. Repository settings

- [ ] Set the repository description:
      `Lucid (Beta) — a minimal, GPU-accelerated code editor built on Vulkan.`
- [ ] Set the homepage to `https://initializeddd.github.io/Lucid/` (or your custom domain).
- [ ] Upload `docs/assets/social-preview.png` under **Settings → General → Social preview**.
- [ ] Review topics: `code-editor`, `cpp`, `cross-platform`, `glfw`, `gpu`, `moltenvk`, `vulkan`,
      `developer-tools`, `text-editor`, `keyboard-first`, `native`, `macos`, `linux`, `windows`.
- [ ] Keep **Issues** and **Discussions** enabled. Decide whether to enable the **Wiki** (docs are
      the single source today).
- [ ] Enable **Projects** if you want a public roadmap board.
- [ ] Confirm **Allow forking** is enabled for a public project.
- [ ] Check that **Releases** are visible and the latest stable and latest pre-release are correct.

## 2. Security and access

- [ ] Enable **Private vulnerability reporting** (Settings → Code security).
- [ ] Enable **Dependabot alerts** and **Dependabot security updates**.
- [ ] Enable **Secret scanning** and **Push protection**.
- [ ] Enable **CodeQL** default setup or keep the committed `codeql.yml` workflow.
- [ ] After the repository is public, add a **branch protection rule** or **ruleset** for `main`:
      require a pull request, one review, the `CI` and `Docs` checks, linear history, and block
      force pushes and deletions.
- [ ] Consider requiring signed commits and a signed-off-by trailer.
- [ ] Review **Environments**, **Deploy keys**, **Webhooks**, **GitHub Apps** and **Actions secrets**
      for anything that should not be exposed.
- [ ] Confirm the `Scorecard` workflow starts publishing results after the repository is public.
- [ ] Enable **Private vulnerability reporting** before the first public release announcement.

## 3. Documentation site

- [ ] Make the repository public first (Pages on a free plan requires a public repository).
- [ ] Set **Settings → Pages → Build and deployment → Source** to **GitHub Actions**.
- [ ] Run the `Docs` workflow or push a change under `docs/` to deploy.
- [ ] Verify `https://initializeddd.github.io/Lucid/` loads and the screenshots render.
- [ ] Update `site_url` in `mkdocs.yml` if you use a custom domain.
- [ ] If you use a custom domain, add `docs/CNAME` and configure the DNS records.
- [ ] Check that the README and `docs/` links do not point at private-only pages.

## 4. Content cleanup

- [ ] Search for private-only wording:
      `rg -n "private repository|requires GitHub access|current private" README.md SECURITY.md docs .github`
- [ ] Confirm the README screenshots and `docs/assets/screenshots/` files are current.
- [ ] Update `CHANGELOG.md` with the release that will be public.
- [ ] Confirm third-party notices are complete (`assets/licenses/`, `assets/fonts/`).
- [ ] Confirm generated background assets are redistributable.
- [ ] Remove internal notes, TODOs and experiments that should not be public.
- [ ] Check for secrets in history: `gitleaks detect --source . --verbose` (install with
      `brew install gitleaks`).
- [ ] Check repository size: `git-sizer --verbose` and `find . -type f -size +5M`.
- [ ] Rotate any credential that ever appeared in history. Public history is permanent.

## 5. CI and releases

- [ ] Confirm `CI` passes on macOS, Linux and Windows.
- [ ] Confirm `BSD` passes.
- [ ] Confirm `Docs` builds with `mkdocs build --strict`.
- [ ] Confirm `CodeQL` and `Scorecard` are green after the repository is public.
- [ ] Confirm the release workflow produces the expected artifacts and SHA-256 checksums.
- [ ] Decide whether to add code signing/notarization for macOS and Windows.
- [ ] Consider adding build provenance (`actions/attest-build-provenance`) and an SBOM.
- [ ] Tag the first public release and write public release notes.

## 6. Distribution and discoverability

- [ ] Publish a Homebrew cask or tap for macOS.
- [ ] Publish a winget manifest for Windows.
- [ ] Publish an AUR package and/or Flathub listing for Linux.
- [ ] Verify the install script downloads public release assets.
- [ ] Submit to relevant `awesome-*` lists and package directories.
- [ ] Prepare a launch post for Hacker News, Product Hunt, Reddit and your own channels.
- [ ] Add a star-history chart and a contributors image after the repository is public:
      - `https://api.star-history.com/svg?repos=initializeddd/Lucid&type=Date`
      - `https://contrib.rocks/image?repo=initializeddd/Lucid`
- [ ] Add a `FUNDING.yml` entry if you want a Sponsor button.

## 7. Community

- [ ] Seed **good first issue** and **help wanted** issues.
- [ ] Create Discussions categories (Announcements, Q&A, Ideas, Show and tell).
- [ ] Confirm `CODE_OF_CONDUCT.md` enforcement contact is current.
- [ ] Confirm `CODEOWNERS` points at the right account.
- [ ] Confirm `SUPPORT.md`, `SECURITY.md` and issue forms are accurate.
- [ ] Add a moderation plan for issues and Discussions.

## 8. Final pre-flight

Run these from a clean checkout before changing visibility:

```sh
git status
git log --oneline -10
git ls-files | wc -l
gitleaks detect --source . --verbose
git-sizer --verbose
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
pip install -r docs/requirements.txt
mkdocs build --strict
```

When everything above is ready, make the repository public:

```sh
gh repo edit initializeddd/Lucid --visibility public --accept-visibility-change-consequences
```

Then enable Pages, branch protection, CodeQL and Scorecard, and verify the docs URL and social
preview.
