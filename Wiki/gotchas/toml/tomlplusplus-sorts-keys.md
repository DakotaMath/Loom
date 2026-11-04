---
type: gotcha
tags: [toml, tomlplusplus, front-matter]
---

# toml++ writes table keys in alphabetical order

> **Summary:** A `toml::table` in toml++ is a `std::map`, so it forgets the order keys were read or inserted in and writes them alphabetically. Never write Loom's files with toml++'s formatter; use Loom's own writer, which puts fields in the order the spec lists.

**Last verified:** 2026-11-04 · commit 9693149

## Symptom
A file parsed and written back with toml++ comes out with its keys reordered (`created` before `id`, `premises` before `proves`), so every rewrite shows as a large diff.

## Cause
In toml++ v3, `toml::table` stores its entries in `std::map<toml::key, node_ptr, std::less<>>` (`include/toml++/impl/table.hpp`). A `std::map` is sorted by key, so the original order is gone as soon as the file is parsed.

## Fix
Parse with toml++, but write front matter and Loom's TOML files with Loom's own writer, field by field in the order given in [`Specs/m1-core.md`](../../../Specs/m1-core.md) (*Front matter*, canonical form).

Where the file's own order is data (a stub's `[locators]`, the version labels of a `[[renumbering]]` entry), iterating the table gives it alphabetically too. Sort what you read by each key's line, `key.source().begin.line`, as `readStub()` and `PaperReader::renumberings()` in `Source/LoomCore/Papers.cpp` do.

## See also
- Up: [Gotchas](../README.md)
