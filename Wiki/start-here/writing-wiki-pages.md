---
type: detail
tags: [wiki, conventions, templates]
---

# Writing wiki pages

> **Summary:** Every page uses one skeleton: a metadata block, a 3–5 line summary, a "Last verified" line, the body for its page type, and "See also" links. Copy the `_TEMPLATE.md` from the folder you're writing in. Never edit a section's page list: it is generated from each page's title and summary by `python Tools/update_indexes.py`.

**Last verified:** 2026-11-03 · commit c70b3e2

## Shared skeleton

```markdown
---
type: <detail | how-to | gotcha | decision | spec | log>
tags: [<tag>, <tag>]
---

# <Title>

> **Summary:** <3–5 lines. What this page covers and the most important thing to know, so a reader can stop here.>

**Last verified:** <YYYY-MM-DD> · commit <short hash>

<body: defined by the page type>

## See also
- Up: <the section README>
- <link>: <why it's related>
```

- **Summary:** lead with the answer, not background. The section index shows it, so it must stand alone and stay on one line.
- **Last verified:** the date and the commit on `main` the page was checked against. Update it whenever you re-check the page, even if nothing changed.
- **Dates:** US Central, never UTC (`TZ=America/Chicago date +%F` in cloud sessions).
- **One topic per page.** If a page grows two topics, split it.

## Where pages go

| Page type | Folder | Template |
| --- | --- | --- |
| Decision | `Wiki/decisions/`, named `NNN-<topic>.md` | [`decisions/_TEMPLATE.md`](../decisions/_TEMPLATE.md) |
| How-to | `Wiki/how-to/` | [`how-to/_TEMPLATE.md`](../how-to/_TEMPLATE.md) |
| Gotcha | `Wiki/gotchas/<area>/`, one folder per area (`qt`, `build`, `git`, `latex`, …) | [`gotchas/_TEMPLATE.md`](../gotchas/_TEMPLATE.md) |
| Session log | `Wiki/logs/<YYYY-MM>/`, named `<YYYY-MM-DD>-<topic>.md` | [`logs/_TEMPLATE.md`](../logs/_TEMPLATE.md) |
| Spec | `Specs/` (not the wiki) | [`Specs/_TEMPLATE.md`](../../Specs/_TEMPLATE.md) |

Placeholders are in `<angle brackets>`; sections marked *(optional)* can be dropped.

## Indexes are generated
Each section `README.md` has a block between `BEGIN GENERATED` and `END GENERATED` markers. `python Tools/update_indexes.py` rewrites it from every page's title and summary, grouping pages in subfolders (gotchas by area, logs by month, newest first). The checks run it with `--check` and fail when an index is stale, so after adding or changing a page, run it and commit the result.

## See also
- Up: [Start here](README.md)
- [Workflow](workflow.md): how a task becomes a PR
