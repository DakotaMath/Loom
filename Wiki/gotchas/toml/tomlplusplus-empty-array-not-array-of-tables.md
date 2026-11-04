---
type: gotcha
tags: [toml, tomlplusplus, papers]
---

# toml++ says an empty array isn't an array of tables

> **Summary:** In toml++ v3, `array::is_array_of_tables()` (and `is_homogeneous()`) return `false` for an empty array, so `versions = []` fails a check written as "must be an array of tables" with a confusing `wrong-type` message. Test `array->empty()` first, and treat an empty list as "none" (`missing-field` where one is required).

**Last verified:** 2026-11-04 · commit 9693149

## Symptom
A `paper.toml` with `versions = []` (or `reading = []`) is reported as "`versions` should be a list of versions, each `[[versions]]`, not a list", when the real problem is that it has no versions.

## Cause
`toml::array::is_array_of_tables()` is `is_homogeneous(node_type::table)`, and `is_homogeneous()` starts with `if (elems_.empty()) return false;` (`include/toml++/impl/array.inl`). An empty array holds no tables, so toml++ says it isn't an array of them.

## Fix
Check for an empty array before asking whether it is an array of tables, as `PaperReader::versions()` in `Source/LoomCore/Papers.cpp` does: `if (!array || (!array->empty() && !array->is_array_of_tables()))` is the wrong-type case; an empty array is then the same as a missing one.

## See also
- Up: [Gotchas](../README.md)
- [toml++ writes table keys in alphabetical order](tomlplusplus-sorts-keys.md): the other toml++ trap Loom's readers work around
