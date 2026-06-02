"""Replace text in Microsoft Word documents using Word's VBA-compatible API."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path
from typing import Callable, Iterable, Iterator, Mapping, Sequence


# Word constants. Keeping them local avoids requiring generated pywin32 wrappers.
WD_FIND_CONTINUE = 1
WD_REPLACE_ALL = 2
STRUCTURAL_TEXT_MARKERS = ("^p", "^13", "^l", "^11")
WORD_EXTENSIONS = {".doc", ".docx", ".docm"}


def replacement_pair(value: str) -> tuple[str, str]:
    """Parse OLD=NEW while allowing equals signs in the replacement text."""
    if "=" not in value:
        raise argparse.ArgumentTypeError("replacement must use the form OLD=NEW")
    old, new = value.split("=", 1)
    if not old:
        raise argparse.ArgumentTypeError("OLD text cannot be empty")
    return old, new


def load_replacements(
    pairs: Iterable[tuple[str, str]], json_file: Path | None
) -> dict[str, str]:
    replacements: dict[str, str] = {}
    if json_file:
        try:
            data = json.loads(json_file.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise ValueError(f"cannot read replacements JSON: {exc}") from exc
        if not isinstance(data, dict) or not all(
            isinstance(key, str) and isinstance(value, str)
            for key, value in data.items()
        ):
            raise ValueError("replacements JSON must be an object of string keys and values")
        replacements.update(data)
    replacements.update(pairs)
    validate_text_only_replacements(replacements)
    return replacements


def validate_text_only_replacements(replacements: Mapping[str, str]) -> None:
    if not replacements:
        raise ValueError("provide at least one replacement")
    if "" in replacements:
        raise ValueError("replacement search text cannot be empty")
    for old, new in replacements.items():
        for value in (old, new):
            lowered = value.lower()
            if "\r" in value or "\n" in value or any(
                marker in lowered for marker in STRUCTURAL_TEXT_MARKERS
            ):
                raise ValueError(
                    "text-only mode does not allow paragraph or line-break replacements"
                )


def save_replacements(replacements: Mapping[str, str], json_file: Path) -> None:
    validate_text_only_replacements(replacements)
    json_file.parent.mkdir(parents=True, exist_ok=True)
    json_file.write_text(
        json.dumps(dict(replacements), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def create_backup(input_path: Path) -> Path:
    backup_path = input_path.with_suffix(f"{input_path.suffix}.bak")
    shutil.copy2(input_path, backup_path)
    return backup_path


def resolve_output(input_path: Path, output_path: Path | None, in_place: bool) -> Path:
    input_path = input_path.resolve()
    if in_place:
        return input_path
    if output_path:
        resolved_output = output_path.resolve()
        if resolved_output == input_path:
            raise ValueError("use --in-place to overwrite the input document")
        return resolved_output
    return input_path.with_name(f"{input_path.stem}.replaced{input_path.suffix}")


def iter_story_ranges(document: object) -> Iterator[object]:
    """Yield body, header, footer, note, and linked story ranges."""
    for story_range in document.StoryRanges:
        current = story_range
        while current is not None:
            yield current
            current = current.NextStoryRange


def replace_in_range(word_range: object, old: str, new: str, wildcards: bool) -> bool:
    """Replace text while inheriting the matched text's existing Word formatting."""
    find = word_range.Find
    find.ClearFormatting()
    find.Replacement.ClearFormatting()
    return bool(
        find.Execute(
            FindText=old,
            MatchCase=False,
            MatchWholeWord=False,
            MatchWildcards=wildcards,
            MatchSoundsLike=False,
            MatchAllWordForms=False,
            Forward=True,
            Wrap=WD_FIND_CONTINUE,
            Format=False,
            ReplaceWith=new,
            Replace=WD_REPLACE_ALL,
        )
    )


def replace_in_story_range(
    story_range: object,
    replacements: Mapping[str, str],
    wildcards: bool,
) -> Iterator[tuple[str, bool]]:
    """Apply rules while avoiding Word scans for literal text that is absent."""
    cached_text = None
    cached_text_casefold = None
    if not wildcards:
        try:
            cached_text = str(story_range.Text)
            cached_text_casefold = cached_text.casefold()
        except Exception:
            # Some COM story ranges do not expose readable text. Word Find still works.
            pass

    for old, new in replacements.items():
        should_execute = (
            cached_text is None
            or "^" in old
            or old.casefold() in cached_text_casefold
        )
        replaced = False
        if should_execute:
            replaced = replace_in_range(story_range, old, new, wildcards)
            if replaced and cached_text is not None:
                try:
                    # Refresh after a match so later ordered rules can match new text.
                    cached_text = str(story_range.Text)
                    cached_text_casefold = cached_text.casefold()
                except Exception:
                    cached_text = None
                    cached_text_casefold = None
        yield old, replaced


def replace_in_document(
    input_path: Path,
    output_path: Path,
    replacements: Mapping[str, str],
    *,
    wildcards: bool,
    visible: bool,
    progress_callback: Callable[[int, int, str], None] | None = None,
) -> None:
    validate_text_only_replacements(replacements)
    try:
        import pythoncom
        import win32com.client
    except ImportError as exc:
        raise RuntimeError("pywin32 is required; run: python -m pip install pywin32") from exc

    pythoncom.CoInitialize()
    word = None
    document = None
    try:
        word = win32com.client.DispatchEx("Word.Application")
        word.Visible = visible
        word.DisplayAlerts = 0
        word.ScreenUpdating = False
        document = word.Documents.Open(str(input_path.resolve()))

        story_ranges = list(iter_story_ranges(document))
        total_steps = len(story_ranges) * len(replacements)
        completed_steps = 0
        if progress_callback:
            progress_callback(completed_steps, total_steps, "")
        for story_range in story_ranges:
            for old, _replaced in replace_in_story_range(
                story_range, replacements, wildcards
            ):
                completed_steps += 1
                if progress_callback:
                    progress_callback(completed_steps, total_steps, old)

        output_path.parent.mkdir(parents=True, exist_ok=True)
        if output_path.resolve() == input_path.resolve():
            document.Save()
        else:
            # SaveAs2 preserves the input type when the extension is unchanged.
            document.SaveAs2(str(output_path.resolve()))
    finally:
        if document is not None:
            document.Close(SaveChanges=False)
        if word is not None:
            word.Quit()
        pythoncom.CoUninitialize()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Replace text in a Word document using the same Word Find/Replace "
            "object model exposed to VBA."
        )
    )
    parser.add_argument("input", type=Path, help="input .doc, .docx, or .docm file")
    parser.add_argument("-o", "--output", type=Path, help="output file path")
    parser.add_argument(
        "-r",
        "--replace",
        action="append",
        default=[],
        type=replacement_pair,
        metavar="OLD=NEW",
        help="replacement to apply; may be specified multiple times",
    )
    parser.add_argument(
        "--replacements-json",
        type=Path,
        help='UTF-8 JSON file such as {"old": "new"}',
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="overwrite the input document after making a .bak copy",
    )
    parser.add_argument(
        "--wildcards",
        action="store_true",
        help="interpret OLD values as Word wildcard patterns",
    )
    parser.add_argument(
        "--visible",
        action="store_true",
        help="show Microsoft Word while processing",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        input_path = args.input.resolve()
        if not input_path.is_file():
            raise ValueError(f"input document does not exist: {input_path}")
        if input_path.suffix.lower() not in WORD_EXTENSIONS:
            raise ValueError("input must be a .doc, .docx, or .docm file")
        output_path = resolve_output(input_path, args.output, args.in_place)
        replacements = load_replacements(args.replace, args.replacements_json)
        if args.in_place:
            backup_path = create_backup(input_path)
            print(f"Backup: {backup_path}")
        replace_in_document(
            input_path,
            output_path,
            replacements,
            wildcards=args.wildcards,
            visible=args.visible,
        )
    except (OSError, RuntimeError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Created: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
