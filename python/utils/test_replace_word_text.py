import argparse
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch

from replace_word_text import (
    create_backup,
    load_replacements,
    replace_in_document,
    replace_in_range,
    replace_in_story_range,
    replacement_pair,
    resolve_output,
    save_replacements,
    validate_text_only_replacements,
)


class ReplaceWordTextTests(unittest.TestCase):
    def test_create_backup_copies_input_next_to_document(self):
        with tempfile.TemporaryDirectory() as directory:
            input_path = Path(directory) / "example.docx"
            input_path.write_bytes(b"document")
            backup_path = create_backup(input_path)
            self.assertEqual(backup_path, Path(directory) / "example.docx.bak")
            self.assertEqual(backup_path.read_bytes(), b"document")

    def test_replacement_pair_splits_only_first_equals(self):
        self.assertEqual(replacement_pair("a=b=c"), ("a", "b=c"))

    def test_replacement_pair_requires_old_text(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            replacement_pair("=new")

    def test_load_replacements_merges_json_and_cli_with_cli_winning(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "replacements.json"
            path.write_text(json.dumps({"a": "json", "b": "value"}), encoding="utf-8")
            replacements = load_replacements([("a", "cli")], path)
        self.assertEqual(replacements, {"a": "cli", "b": "value"})

    def test_default_output_keeps_extension(self):
        input_path = Path("example.docx").resolve()
        self.assertEqual(
            resolve_output(input_path, None, False),
            input_path.with_name("example.replaced.docx"),
        )

    def test_explicit_same_output_requires_in_place(self):
        input_path = Path("example.docx").resolve()
        with self.assertRaises(ValueError):
            resolve_output(input_path, input_path, False)

    def test_save_replacements_round_trips_unicode_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "configs" / "replacements.json"
            save_replacements({"{{NAME}}": "李明", "draft": "final"}, path)
            replacements = load_replacements([], path)
        self.assertEqual(replacements, {"{{NAME}}": "李明", "draft": "final"})

    def test_save_replacements_requires_at_least_one_value(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "replacements.json"
            with self.assertRaises(ValueError):
                save_replacements({}, path)

    def test_text_only_replacements_reject_paragraph_markers(self):
        with self.assertRaises(ValueError):
            validate_text_only_replacements({"^p": " "})
        with self.assertRaises(ValueError):
            validate_text_only_replacements({"old": "new\nparagraph"})

    def test_replace_in_range_does_not_apply_formatting(self):
        class Recorder:
            def __init__(self):
                self.cleared = False
                self.execute_args = None

            def ClearFormatting(self):
                self.cleared = True

        class FindRecorder(Recorder):
            def __init__(self):
                super().__init__()
                self.Replacement = Recorder()

            def Execute(self, **kwargs):
                self.execute_args = kwargs
                return True

        class RangeRecorder:
            def __init__(self):
                self.Find = FindRecorder()

        word_range = RangeRecorder()
        replace_in_range(word_range, "draft", "final", False)
        self.assertTrue(word_range.Find.cleared)
        self.assertTrue(word_range.Find.Replacement.cleared)
        self.assertFalse(word_range.Find.execute_args["Format"])
        self.assertEqual(word_range.Find.execute_args["ReplaceWith"], "final")

    def test_replace_in_story_range_skips_absent_literal_rules(self):
        class Recorder:
            def ClearFormatting(self):
                pass

        class FindRecorder(Recorder):
            def __init__(self):
                self.Replacement = Recorder()
                self.calls = []

            def Execute(self, **kwargs):
                self.calls.append(kwargs["FindText"])
                return True

        class StoryRange:
            def __init__(self):
                self.Find = FindRecorder()
                self.Text = "Existing draft text"

        story_range = StoryRange()
        results = list(
            replace_in_story_range(
                story_range,
                {"missing": "unused", "draft": "final"},
                wildcards=False,
            )
        )
        self.assertEqual(story_range.Find.calls, ["draft"])
        self.assertEqual(results, [("missing", False), ("draft", True)])

    def test_replace_in_story_range_scans_every_rule_in_wildcard_mode(self):
        class Recorder:
            def ClearFormatting(self):
                pass

        class FindRecorder(Recorder):
            def __init__(self):
                self.Replacement = Recorder()
                self.calls = []

            def Execute(self, **kwargs):
                self.calls.append(kwargs["FindText"])
                return False

        class StoryRange:
            def __init__(self):
                self.Find = FindRecorder()
                self.Text = "Existing text"

        story_range = StoryRange()
        list(replace_in_story_range(story_range, {"m*ing": "unused"}, wildcards=True))
        self.assertEqual(story_range.Find.calls, ["m*ing"])

    def test_replace_in_story_range_refreshes_cache_for_ordered_rules(self):
        class Recorder:
            def ClearFormatting(self):
                pass

        class FindRecorder(Recorder):
            def __init__(self, story_range):
                self.story_range = story_range
                self.Replacement = Recorder()
                self.calls = []

            def Execute(self, **kwargs):
                self.calls.append(kwargs["FindText"])
                self.story_range.Text = self.story_range.Text.replace(
                    kwargs["FindText"], kwargs["ReplaceWith"]
                )
                return True

        class StoryRange:
            def __init__(self):
                self.Text = "draft"
                self.Find = FindRecorder(self)

        story_range = StoryRange()
        list(
            replace_in_story_range(
                story_range,
                {"draft": "review", "review": "final"},
                wildcards=False,
            )
        )
        self.assertEqual(story_range.Find.calls, ["draft", "review"])
        self.assertEqual(story_range.Text, "final")

    def test_replace_in_document_reports_progress_for_each_rule(self):
        class Recorder:
            def ClearFormatting(self):
                pass

        class FindRecorder(Recorder):
            def __init__(self):
                self.Replacement = Recorder()

            def Execute(self, **_kwargs):
                pass

        class StoryRange:
            def __init__(self):
                self.Find = FindRecorder()
                self.NextStoryRange = None
                self.Text = "draft old"

        class Document:
            def __init__(self):
                self.StoryRanges = [StoryRange()]

            def SaveAs2(self, _path):
                pass

            def Close(self, **_kwargs):
                pass

        class Documents:
            def __init__(self):
                self.document = Document()

            def Open(self, _path):
                return self.document

        class Word:
            def __init__(self):
                self.Documents = Documents()

            def Quit(self):
                pass

        pythoncom = types.ModuleType("pythoncom")
        pythoncom.CoInitialize = lambda: None
        pythoncom.CoUninitialize = lambda: None
        win32com = types.ModuleType("win32com")
        win32com_client = types.ModuleType("win32com.client")
        win32com_client.DispatchEx = lambda _name: Word()
        win32com.client = win32com_client

        progress = []
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            input_path = root / "input.docx"
            input_path.touch()
            modules = {
                "pythoncom": pythoncom,
                "win32com": win32com,
                "win32com.client": win32com_client,
            }
            with patch.dict(sys.modules, modules):
                replace_in_document(
                    input_path,
                    root / "output.docx",
                    {"draft": "final", "old": "new"},
                    wildcards=False,
                    visible=False,
                    progress_callback=lambda completed, total, text: progress.append(
                        (completed, total, text)
                    ),
                )

        self.assertEqual(
            progress,
            [(0, 2, ""), (1, 2, "draft"), (2, 2, "old")],
        )


if __name__ == "__main__":
    unittest.main()
