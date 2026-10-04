#!/usr/bin/env python3
"""Insert the GoatCounter snippet into generated HTML.

The Jekyll pages get their snippet from docs/_includes/custom-foot.html, which
the theme pulls into every layout. The Sphinx and Doxygen reference trees are
not Jekyll, so nothing in docs/ can reach them -- this script does, as a step
of the deployment workflow after both have been generated.

Injecting here rather than through each tool's own templates is deliberate.
Sphinx can add a script through html_js_files, but Doxygen has no equivalent:
its only hook is a complete HTML_FOOTER template, which is coupled to the
Doxygen version and silently drops the navigation path when the two drift.
One step covering both outputs has no such coupling and keeps the whole
arrangement in one place. It also runs only in CI, so building the reference
locally never registers page views -- matching the jekyll.environment guard on
the Jekyll side.

Usage:  add-analytics.py <directory> [<directory> ...]

Exits non-zero if a directory holds no HTML, or if any page is left without
the snippet, so the step cannot quietly become a no-op.
"""

import sys
from pathlib import Path

SNIPPET = (
    '<script data-goatcounter="https://booritas.goatcounter.com/count"\n'
    '        async src="//gc.zgo.at/count.js"></script>\n'
)

MARKER = "goatcounter"


def inject(html: str) -> "str | None":
    """Return html with the snippet before the final </body>, or None.

    None means no change is needed or possible: the snippet is already there,
    or the document has no closing body tag (Doxygen writes a few fragments
    that are included into other pages rather than served on their own).
    """
    if MARKER in html:
        return None
    index = html.lower().rfind("</body>")
    if index == -1:
        return None
    return html[:index] + SNIPPET + html[index:]


def main(argv: "list[str]") -> int:
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2

    total_added = 0
    total_pages = 0
    failures = []

    for raw in argv[1:]:
        directory = Path(raw)
        if not directory.is_dir():
            print(f"error: not a directory: {directory}", file=sys.stderr)
            return 1

        pages = sorted(directory.rglob("*.html"))
        if not pages:
            print(f"error: no HTML found under {directory}", file=sys.stderr)
            return 1

        added = skipped = 0
        for page in pages:
            html = page.read_text(encoding="utf-8", errors="surrogateescape")
            updated = inject(html)
            if updated is None:
                skipped += 1
                # A page without </body> is a fragment and is not served; a
                # page that already carries the snippet is fine. Only a served
                # page missing it is a failure, and that is what we check for
                # below.
                if MARKER not in html and "</body>" in html.lower():
                    failures.append(page)
                continue
            page.write_text(updated, encoding="utf-8", errors="surrogateescape")
            added += 1

        total_added += added
        total_pages += len(pages)
        print(f"{directory}: {added} page(s) updated, {skipped} skipped, "
              f"{len(pages)} total")

    if failures:
        print(f"error: {len(failures)} page(s) left without the snippet, "
              f"first: {failures[0]}", file=sys.stderr)
        return 1

    if total_added == 0:
        print("error: nothing was updated; the snippet may already be applied "
              "by another mechanism, or the paths are wrong", file=sys.stderr)
        return 1

    print(f"added the analytics snippet to {total_added} of {total_pages} page(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
