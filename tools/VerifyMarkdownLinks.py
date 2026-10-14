"""
VerifyMarkdownLinks.py
Validates every internal relative link in all markdown documentation files,
correctly ignoring code blocks and inline code.
"""

import re
import pathlib
import sys


def strip_code_blocks(text):
    # Remove fenced code blocks ``` ... ```
    text = re.sub(r"```[\s\S]*?```", "", text)
    # Remove inline code ` ... `
    text = re.sub(r"`[^`]*`", "", text)
    return text


def verify_links():
    root = pathlib.Path(__file__).resolve().parent.parent
    md_files = list(root.rglob("*.md"))
    broken = []
    total_links = 0

    for md_path in md_files:
        try:
            raw_content = md_path.read_text(encoding="utf-8", errors="ignore")
        except Exception as e:
            print(f"Error reading {md_path}: {e}")
            continue

        clean_content = strip_code_blocks(raw_content)

        links = re.findall(r"\[([^\]]+)\]\(([^)]+)\)", clean_content)
        for text, link in links:
            link = link.strip()
            if (
                link.startswith("http://")
                or link.startswith("https://")
                or link.startswith("#")
                or link.startswith("mailto:")
            ):
                continue
            total_links += 1
            target_path = link.split("#")[0]
            if not target_path:
                continue

            # Resolve relative to the file's parent folder
            resolved = (md_path.parent / target_path).resolve()
            if not resolved.exists():
                broken.append((str(md_path.relative_to(root)), link, str(resolved)))

    print(f"Checked {len(md_files)} markdown files with {total_links} relative documentation links.")
    if broken:
        print(f"Found {len(broken)} broken link(s):")
        for filePath, linkText, resolvedPath in broken:
            print(f"  In {filePath}: link '{linkText}' -> target does not exist at {resolvedPath}")
        return False
    else:
        print("[SUCCESS] All relative documentation links are 100% valid and verified on disk!")
        return True


if __name__ == "__main__":
    success = verify_links()
    sys.exit(0 if success else 1)
