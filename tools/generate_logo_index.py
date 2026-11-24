"""Build Allfetch's portable JSON index from Fastfetch's logo registry."""

import argparse
import json
import re
import string
from pathlib import Path
from tempfile import NamedTemporaryFile


DEFAULT_ROOT = Path(__file__).resolve().parents[1] / "assets/logos/fastfetch"
PREFIX = "FASTFETCH_DATATEXT_LOGO_"
SMALL = "FF_LOGO_LINE_TYPE_SMALL_BIT"
ALTERNATE = "FF_LOGO_LINE_TYPE_ALTER_BIT"
COLOR_NAMES = ("BLACK", "RED", "GREEN", "YELLOW", "BLUE", "MAGENTA", "CYAN", "WHITE")
COLOR_CODES = {
    **{f"FF_COLOR_FG_{name}": str(30 + i) for i, name in enumerate(COLOR_NAMES)},
    **{f"FF_COLOR_FG_LIGHT_{name}": str(90 + i) for i, name in enumerate(COLOR_NAMES)},
    "FF_COLOR_FG_DEFAULT": "39",
    "FF_COLOR_FG_256": "38;5;",
    "FF_COLOR_FG_RGB": "38;2;",
    "FF_COLOR_BG_WHITE": "47",
    "FF_COLOR_MODE_BOLD": "1;",
}


def read_colors(block):
    field = re.search(r"\.colors\s*=\s*\{([^}]*)\}", block)
    if not field:
        return []

    contents = re.sub(r"//[^\n]*|/\*.*?\*/", "", field[1], flags=re.S)
    colors = []
    for entry in contents.split(","):
        if not entry.strip():
            continue
        tokens = re.findall(r'FF_COLOR_\w+|"[0-9;]+"|\S', entry)
        try:
            colors.append("".join(token[1:-1] if token.startswith('"') else
                                  COLOR_CODES[token] for token in tokens))
        except KeyError as error:
            raise ValueError(f"Unsupported logo color: {entry.strip()}") from error
    if len(colors) > 9 or any(not re.fullmatch(r"[0-9]+(?:;[0-9]+)*", color) for color in colors):
        raise ValueError("Invalid logo color slots")
    return colors


def generate(root, registry):
    artwork = {}
    for path in sorted(root.glob("[a-z_]/*.txt")):
        key = path.stem.upper()
        if key in artwork:
            raise ValueError(f"Duplicate art filename: {path}")
        artwork[key] = path.relative_to(root).as_posix()

    entries = []
    aliases_seen = {}
    used = set()

    def add(key, aliases, flags, colors):
        if key not in artwork:
            raise ValueError(f"Missing art for {key}")
        for alias in aliases:
            identity = (alias.casefold(), frozenset(flags))
            previous = aliases_seen.setdefault(identity, key)
            if previous != key:
                raise ValueError(f"Ambiguous alias {alias!r}: {previous} and {key}")
        used.add(key)
        entries.append(dict(aliases=aliases, path=artwork[key],
                            small=SMALL in flags, alternate=ALTERNATE in flags,
                            colors=colors))

    for letter in string.ascii_lowercase:
        source = (registry / f"{letter}.inc").read_text(encoding="utf-8")
        # Each upstream entry is guarded by its art macro.
        blocks = re.findall(r"#ifdef\s+" + PREFIX + r"(\w+)\b(.*?)#endif", source, re.S)
        if len(blocks) != len(re.findall(r"\.names\s*=", source)):
            raise ValueError(f"Unrecognized registry entries in {letter}.inc")
        for guard, block in blocks:
            names = re.search(r"\.names\s*=\s*\{([^}]*)\}", block)
            lines = re.search(r"\.lines\s*=\s*" + PREFIX + r"(\w+)", block)
            if not names or not lines:
                raise ValueError(f"Incomplete registry entry: {guard}")
            aliases = json.loads("[" + names[1].strip().rstrip(",") + "]")
            if not aliases or not all(isinstance(a, str) and a for a in aliases):
                raise ValueError(f"Invalid aliases for {guard}")
            variant = re.search(r"\.type\s*=\s*([^,]+)", block)
            flags = {flag.strip() for flag in variant[1].split("|")} if variant else set()
            if flags - {SMALL, ALTERNATE}:
                raise ValueError(f"Unsupported logo type for {guard}: {variant[1]}")
            colors = read_colors(block)
            if lines[1] in {"WINDOWS_11", "WINDOWS_11_SMALL"}:
                # Keep Allfetch's cyan accent for the Windows 11 logo.
                colors = [COLOR_CODES["FF_COLOR_FG_CYAN"]] * len(colors)
            add(lines[1], aliases, flags, colors)

    # Upstream declares this fallback separately in builtin.c.
    add("UNKNOWN", ["unknown"], set(), ["39"])
    missing = artwork.keys() - used
    if missing:
        raise ValueError("Unregistered artwork: " + ", ".join(sorted(missing)))
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        epilog="Example: python tools/generate_logo_index.py --output logo_index.json")
    parser.add_argument("--logo-root", type=Path, default=DEFAULT_ROOT,
                        help="art directory (default: repository assets/logos/fastfetch)")
    parser.add_argument("--registry-dir", type=Path,
                        help="directory containing a.inc through z.inc (default: logo root)")
    parser.add_argument("--output", type=Path,
                        help="destination (default: logo_index.json in the logo root)")
    args = parser.parse_args()
    output = args.output or args.logo_root / "logo_index.json"
    temporary = None
    try:
        entries = generate(args.logo_root, args.registry_dir or args.logo_root)
        with NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n",
                                dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(entries, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
        temporary.replace(output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    finally:
        if temporary and temporary.exists():
            temporary.unlink()
    print(f"Wrote {len(entries)} logo entries to {output}")


if __name__ == "__main__":
    main()
