# Start here

The pages to read first: how work is organised and how to write wiki pages. The design itself is [`Specs/loom.md`](../../Specs/loom.md).

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
| Page | Summary |
| --- | --- |
| [Workflow](workflow.md) | Work happens one GitHub issue at a time, one PR per issue, in milestones M1–M12 from `Specs/loom.md`. Each milestone starts with a spec task that writes its spec in `Specs/`; build tasks start only once Dakota has approved that spec. Agents open PRs; Dakota reviews and merges every one. |
| [Writing wiki pages](writing-wiki-pages.md) | Every page uses one skeleton: a metadata block, a 3–5 line summary, a "Last verified" line, the body for its page type, and "See also" links. Copy the `_TEMPLATE.md` from the folder you're writing in. Never edit a section's page list: it is generated from each page's title and summary by `python Tools/update_indexes.py`. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
