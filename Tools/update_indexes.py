"""
Purpose: Generate the page tables in every section README of Wiki/ and Specs/, so no index is
ever edited by hand.

Usage:
    python Tools/update_indexes.py           rewrite the generated tables
    python Tools/update_indexes.py --check   change nothing; exit 1 if any table is stale

Example:
    python Tools/update_indexes.py --check

Notes: A section is a folder holding a README.md with the two markers below. Its table lists every
other .md page in the folder and its subfolders (never README.md or _TEMPLATE files), with the
page's title (its first "# " line) and summary (its "> **Summary:**" line). Pages in subfolders
are grouped under the subfolder's name: gotchas by area, logs by month. Logs are listed newest
first; everything else alphabetically. A page without a summary is an error.
"""

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BEGIN = "<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->"
END = "<!-- END GENERATED -->"
SUMMARY_PREFIX = "> **Summary:**"
NEWEST_FIRST = {"logs"}


@dataclass
class Page:
    """One wiki or spec page, as the index shows it."""

    path: Path
    title: str
    summary: str


def read_page(path: Path) -> Page:
    """Read a page's title and summary.

    Args:
        path: the page's Markdown file.

    Returns:
        The page, with its title and summary text.

    Raises:
        ValueError: if the page has no "# " title line or no summary line.
    """
    title = None
    summary = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if title is None and line.startswith("# "):
            title = line[2:].strip()
        if summary is None and line.startswith(SUMMARY_PREFIX):
            summary = line[len(SUMMARY_PREFIX):].strip()
    if title is None or summary is None:
        missing = "a '# ' title" if title is None else "a '> **Summary:**' line"
        shown = path.relative_to(REPO) if path.is_relative_to(REPO) else path
        raise ValueError(f"{shown.as_posix()}: has no {missing}")
    return Page(path, title, summary)


def is_page(path: Path) -> bool:
    """Whether a file is a page the index lists (not a README or a template)."""
    return path.suffix == ".md" and path.name != "README.md" and not path.name.startswith("_TEMPLATE")


def section_pages(section: Path) -> dict[str, list[Page]]:
    """Collect a section's pages, grouped by subfolder ("" for the section folder itself).

    Args:
        section: the folder holding the section's README.md.

    Returns:
        Group name to its pages, each group sorted (newest first for logs, else by file name).
    """
    groups: dict[str, list[Page]] = {}
    for path in sorted(section.rglob("*.md")):
        if not is_page(path):
            continue
        relative_folder = path.parent.relative_to(section)
        group = relative_folder.parts[0] if relative_folder.parts else ""
        groups.setdefault(group, []).append(read_page(path))
    newest_first = section.name in NEWEST_FIRST
    for pages in groups.values():
        pages.sort(key=lambda page: page.path.name, reverse=newest_first)
    return dict(sorted(groups.items(), reverse=newest_first))


def render_table(section: Path) -> str:
    """Render the generated block for one section: a table per group of pages."""
    lines = []
    groups = section_pages(section)
    if not groups:
        lines.append("No pages yet.")
    for group, pages in groups.items():
        if group:
            lines.append(f"### {group}")
            lines.append("")
        lines.append("| Page | Summary |")
        lines.append("| --- | --- |")
        for page in pages:
            link = page.path.relative_to(section).as_posix()
            lines.append(f"| [{page.title}]({link}) | {page.summary} |")
        lines.append("")
    return "\n".join(lines).rstrip("\n")


def updated_readme(readme: Path) -> str:
    """Return the README's text with its generated block replaced by a fresh table."""
    text = readme.read_text(encoding="utf-8")
    start = text.index(BEGIN) + len(BEGIN)
    end = text.index(END)
    return text[:start] + "\n" + render_table(readme.parent) + "\n" + text[end:]


def section_readmes(root: Path) -> list[Path]:
    """Every README.md under Wiki/ and Specs/ that has the generated-block markers."""
    readmes = []
    for top in ("Wiki", "Specs"):
        for readme in sorted((root / top).rglob("README.md")):
            text = readme.read_text(encoding="utf-8")
            if BEGIN in text and END in text:
                readmes.append(readme)
    return readmes


def main(argv: list[str]) -> int:
    """Rewrite or check every section index. Returns the process exit code."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--check", action="store_true", help="change nothing; exit 1 if any table is stale")
    args = parser.parse_args(argv)

    stale = []
    try:
        for readme in section_readmes(REPO):
            fresh = updated_readme(readme)
            if fresh == readme.read_text(encoding="utf-8"):
                continue
            if args.check:
                stale.append(readme.relative_to(REPO).as_posix())
            else:
                readme.write_text(fresh, encoding="utf-8", newline="\n")
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if stale:
        print("Stale indexes (run python Tools/update_indexes.py):", file=sys.stderr)
        for path in stale:
            print(f"  {path}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
