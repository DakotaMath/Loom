"""Tests for Tools/update_indexes.py: tables built from page summaries, grouping, order, errors."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import update_indexes  # noqa: E402  (needs Tools/ on the path)


def write_page(path: Path, title: str, summary: str | None) -> None:
    """Write a minimal page with a title and, optionally, a summary line."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["---", "type: detail", "---", "", f"# {title}", ""]
    if summary is not None:
        lines.append(f"> **Summary:** {summary}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_readme(section: Path) -> Path:
    """Write a section README with an empty generated block."""
    section.mkdir(parents=True, exist_ok=True)
    readme = section / "README.md"
    readme.write_text(f"# Section\n\nIntro.\n\n{update_indexes.BEGIN}\n{update_indexes.END}\n", encoding="utf-8")
    return readme


def test_table_lists_pages_with_titles_and_summaries(tmp_path: Path) -> None:
    section = tmp_path / "how-to"
    write_readme(section)
    write_page(section / "build.md", "How to build", "Build the app.")
    write_page(section / "_TEMPLATE.md", "How to <do the thing>", "<template>")
    table = update_indexes.render_table(section)
    assert "| [How to build](build.md) | Build the app. |" in table
    assert "_TEMPLATE" not in table
    assert "README" not in table


def test_subfolders_become_groups(tmp_path: Path) -> None:
    section = tmp_path / "gotchas"
    write_page(section / "qt" / "moc.md", "Moc misses a header", "Add it to the target.")
    write_page(section / "git" / "lfs.md", "LFS pointer files", "Run git lfs pull.")
    table = update_indexes.render_table(section)
    assert table.index("### git") < table.index("### qt")
    assert "[Moc misses a header](qt/moc.md)" in table


def test_logs_are_newest_first(tmp_path: Path) -> None:
    section = tmp_path / "logs"
    write_page(section / "2026-10" / "2026-10-30-a.md", "Old", "Old session.")
    write_page(section / "2026-11" / "2026-11-02-b.md", "Newer", "Newer session.")
    write_page(section / "2026-11" / "2026-11-03-c.md", "Newest", "Newest session.")
    table = update_indexes.render_table(section)
    assert table.index("### 2026-11") < table.index("### 2026-10")
    assert table.index("Newest") < table.index("Newer")


def test_updated_readme_keeps_text_outside_the_block(tmp_path: Path) -> None:
    section = tmp_path / "decisions"
    readme = write_readme(section)
    write_page(section / "001-x.md", "001: X", "We chose X.")
    text = update_indexes.updated_readme(readme)
    assert text.startswith("# Section\n\nIntro.\n\n")
    assert "| [001: X](001-x.md) | We chose X. |" in text
    assert text.rstrip().endswith(update_indexes.END)


def test_page_without_summary_is_an_error(tmp_path: Path) -> None:
    section = tmp_path / "how-to"
    write_page(section / "bad.md", "No summary", None)
    with pytest.raises(ValueError, match="Summary"):
        update_indexes.render_table(section)


def test_empty_section_says_so(tmp_path: Path) -> None:
    section = tmp_path / "how-to"
    write_readme(section)
    assert update_indexes.render_table(section) == "No pages yet."
