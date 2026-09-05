# Origin Attribution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add accurate upstream attribution and permission context to the public README without changing source code or assigning a license that the maintainer does not control.

**Architecture:** Insert one self-contained attribution section immediately after the existing disclaimer and add it to the table of contents. The section separates original ownership, permission, derivative maintenance, verified additions, research boundaries, and non-endorsement.

**Tech Stack:** Markdown, Git

## Global Constraints

- Modify `README.md` only during implementation.
- Keep repository documentation in English.
- Do not add or imply a standard open-source license.
- Do not claim ownership of the upstream source.
- Do not alter Git history.

---

### Task 1: Add upstream attribution

**Files:**
- Modify: `README.md`
- Test: Markdown and Git diff checks

**Interfaces:**
- Consumes: The existing disclaimer and table-of-contents structure.
- Produces: A stable `#origin-and-attribution` README anchor and an explicit account of the derivative's provenance.

- [ ] **Step 1: Add the attribution section**

Insert an `Origin and attribution` section after the disclaimer. Link to `un4ckn0wl3z/HerculesAC` and revision `df90313`, state the educational public-redistribution permission confirmed by the maintainer, identify `jjnikom576` as the modification maintainer, list the signed manifest, dynamic kernel protection, targeted injection, telemetry, CI, tests, and hardening work, and limit the bypass fixture to owned isolated labs.

- [ ] **Step 2: Update the table of contents**

Add `Origin and attribution` as the first entry and renumber the remaining entries while preserving their existing anchors.

- [ ] **Step 3: Verify documentation**

Run:

```powershell
rg -n "Origin and attribution|un4ckn0wl3z|df90313|published with permission|owned, isolated" README.md
git diff --check
git diff -- README.md
```

Expected: every required statement is present, Markdown whitespace checks pass, and only the intended README sections have changed.

- [ ] **Step 4: Commit and push**

Run:

```powershell
git add -- README.md docs/superpowers/plans/2026-09-05-origin-attribution.md
git commit -m "docs: credit HerculesAC upstream"
git push origin main
git status --short --branch
```

Expected: the commit succeeds, `origin/main` advances to the new commit, and the working tree is clean and synchronized.
