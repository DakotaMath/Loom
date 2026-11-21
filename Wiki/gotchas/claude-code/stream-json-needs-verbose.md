---
type: gotcha
tags: [claude-code, headless, stream-json, jobs, cost]
---

# Claude Code's `stream-json` output needs `--verbose`, and its cost is an API-price estimate

> **Summary:** `claude -p --output-format stream-json` without `--verbose` exits 1 with *"When using --print, --output-format=stream-json requires --verbose"* before doing anything; always pass both. On a subscription, the `result` event's `total_cost_usd` is still filled in, at API list prices (`costBasis: "list"`): it measures a job's size, not money Dakota pays. Usage limits come in `rate_limit_event` lines instead.

**Last verified:** 2026-11-06 · commit cb0a446 · Claude Code 2.1.318

## Symptom
- A headless run with `--output-format stream-json` and no `--verbose` prints only `Error: When using --print, --output-format=stream-json requires --verbose` and exits 1. No events, no session.
- A run that works reports `"total_cost_usd": 0.0094` for a one-word reply on a Max subscription, which looks like a charge.

## Cause
- `stream-json` emits every turn's events, which Claude Code counts as verbose output; it refuses the combination rather than quietly emitting less.
- Claude Code computes `total_cost_usd` (and `modelUsage.<model>.costUSD`) from token counts at list prices whatever the sign-in. The subscription's real limits are its usage windows, reported in `rate_limit_event` lines: `rate_limit_info.status` (`allowed`, `allowed_warning`, `rejected`), `resetsAt` (Unix seconds), `utilization`, and per window (`five_hour`, `seven_day`) in `unifiedWindows`.

## Fix
- Run jobs as `claude -p --verbose --output-format stream-json …` (`Specs/m4-tasks-jobs.md`, *The command line*).
- Show the cost as *≈ $1.23 at API prices*, and the usage windows beside it, from the newest `rate_limit_event` (*The queue*).
- `claude auth status --json` (no model call) says whether Claude Code is signed in (`loggedIn`) and how (`authMethod: "claude.ai"` for a subscription, with `subscriptionType`).

## See also
- Up: [Gotchas](../README.md)
- [The M4 spec](../../../Specs/m4-tasks-jobs.md): running a job, the queue, *Check my setup*
