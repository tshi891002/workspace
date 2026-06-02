# Word Text Replacer

`replace_word_text.py` replaces text in Microsoft Word documents from Python.
It automates Word through COM and calls the same `Range.Find.Execute` API used
by VBA. Replacements are applied to the body and Word story ranges such as
headers and footers. Replacement text inherits the existing Word formatting.
Text-only mode rejects paragraph and line-break replacement markers so configs
cannot accidentally change document structure or styles.

## Requirements

- Windows
- Microsoft Word installed
- Python 3.9+

Install the Python dependency:

```powershell
py -3 -m pip install -r requirements.txt
```

## GUI

Launch the Tkinter desktop application:

```powershell
py -3 .\word_text_replacer_gui.py
```

Choose an input document, add one or more replacements, and click
`Replace text`. Double-click a replacement row to edit it. The GUI can also
load replacements from a UTF-8 JSON file or export the current replacement
rows as a reusable JSON configuration. A progress bar and live status text
show the current replacement step while Word processes the document.

For large documents, literal-text mode caches each Word story range and skips
Word `Find.Execute` scans for values that are not present. Ordered replacements
still work because the cache is refreshed after successful changes. Wildcard
mode uses Word's full scan behavior for correctness.

## Usage

Create `contract.replaced.docx` while leaving the source untouched:

```powershell
py -3 .\replace_word_text.py .\contract.docx `
  --replace "Old Company=New Company" `
  --replace "2025=2026"
```

Choose the output file:

```powershell
py -3 .\replace_word_text.py .\contract.docx `
  --output .\finished.docx `
  --replace "draft=final"
```

Use a UTF-8 JSON file for many replacements:

```json
{
  "{{CLIENT_NAME}}": "Contoso Ltd.",
  "{{DATE}}": "June 2, 2026"
}
```

```powershell
py -3 .\replace_word_text.py .\template.docx `
  --replacements-json .\replacements.json
```

To modify the original file, pass `--in-place`. The program writes a `.bak`
copy first:

```powershell
py -3 .\replace_word_text.py .\contract.docx `
  --in-place `
  --replace "draft=final"
```

Use `--wildcards` when the search values use Word wildcard syntax. Use
`--visible` to watch Word process the document.
