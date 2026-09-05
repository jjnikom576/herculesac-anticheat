# Origin Attribution Design

## Goal

Make the repository's origin, permission basis, maintenance ownership, and defensive research scope clear without implying that the maintainer authored the original HerculesAC code or can relicense it unilaterally.

## Scope

- Add an `Origin and attribution` section near the top of `README.md`.
- Link to the upstream HerculesAC repository and the upstream revision used as the import base.
- State that the original author granted permission to modify and publicly redistribute this derivative for educational purposes.
- Identify Nikom Kawchoem (`jjnikom576`) as the maintainer of the modifications.
- Summarize the major defensive additions that are already evidenced by the repository history.
- Clarify that the experimental bypass fixture is limited to owned, isolated test environments and does not imply endorsement by the upstream author.
- Add the new section to the README table of contents.

## Constraints

- Do not add a standard open-source license on behalf of the upstream author.
- Do not claim ownership of the original source code.
- Do not change source code, build files, history, or existing third-party notices.
- Keep all repository documentation in English.
- Use only claims supported by the repository history and the maintainer's confirmation of permission.

## Verification

- Confirm the README links and Markdown headings are valid.
- Confirm the table of contents matches the new heading.
- Confirm the diff contains documentation changes only.
- Confirm the working tree is clean after commit and the remote branch contains the new commit after push.
