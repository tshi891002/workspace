"""Tkinter desktop interface for replacing text in Microsoft Word documents."""

from __future__ import annotations

import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from replace_word_text import (
    WORD_EXTENSIONS,
    create_backup,
    load_replacements,
    replace_in_document,
    resolve_output,
    save_replacements,
    validate_text_only_replacements,
)


WORD_FILE_TYPES = [
    ("Word documents", "*.docx *.docm *.doc"),
    ("All files", "*.*"),
]


class WordTextReplacerApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Word Text Replacer")
        self.root.geometry("840x590")
        self.root.minsize(720, 500)

        self.input_var = tk.StringVar()
        self.output_var = tk.StringVar()
        self.find_var = tk.StringVar()
        self.replace_var = tk.StringVar()
        self.in_place_var = tk.BooleanVar()
        self.wildcards_var = tk.BooleanVar()
        self.visible_var = tk.BooleanVar()
        self.progress_var = tk.DoubleVar()
        self.status_var = tk.StringVar(value="Choose a Word document and add replacements.")
        self._last_progress_report = 0.0

        self._build_widgets()

    def _build_widgets(self) -> None:
        main = ttk.Frame(self.root, padding=14)
        main.pack(fill="both", expand=True)
        main.columnconfigure(1, weight=1)
        main.rowconfigure(3, weight=1)

        ttk.Label(main, text="Input document").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Entry(main, textvariable=self.input_var).grid(
            row=0, column=1, sticky="ew", padx=8, pady=4
        )
        ttk.Button(main, text="Browse...", command=self._browse_input).grid(
            row=0, column=2, pady=4
        )

        ttk.Label(main, text="Output document").grid(row=1, column=0, sticky="w", pady=4)
        self.output_entry = ttk.Entry(main, textvariable=self.output_var)
        self.output_entry.grid(row=1, column=1, sticky="ew", padx=8, pady=4)
        self.output_button = ttk.Button(main, text="Browse...", command=self._browse_output)
        self.output_button.grid(row=1, column=2, pady=4)

        entry_frame = ttk.LabelFrame(main, text="Add or edit replacement", padding=10)
        entry_frame.grid(row=2, column=0, columnspan=3, sticky="ew", pady=(12, 4))
        entry_frame.columnconfigure(1, weight=1)
        entry_frame.columnconfigure(3, weight=1)
        ttk.Label(entry_frame, text="Find").grid(row=0, column=0, sticky="w")
        ttk.Entry(entry_frame, textvariable=self.find_var).grid(
            row=0, column=1, sticky="ew", padx=(6, 12)
        )
        ttk.Label(entry_frame, text="Replace with").grid(row=0, column=2, sticky="w")
        ttk.Entry(entry_frame, textvariable=self.replace_var).grid(
            row=0, column=3, sticky="ew", padx=6
        )
        ttk.Button(entry_frame, text="Add", command=self._add_replacement).grid(
            row=0, column=4, padx=(6, 0)
        )

        tree_frame = ttk.Frame(main)
        tree_frame.grid(row=3, column=0, columnspan=3, sticky="nsew", pady=4)
        tree_frame.columnconfigure(0, weight=1)
        tree_frame.rowconfigure(0, weight=1)
        self.tree = ttk.Treeview(
            tree_frame,
            columns=("find", "replace"),
            show="headings",
            selectmode="browse",
        )
        self.tree.heading("find", text="Find")
        self.tree.heading("replace", text="Replace with")
        self.tree.column("find", width=350)
        self.tree.column("replace", width=350)
        self.tree.grid(row=0, column=0, sticky="nsew")
        scrollbar = ttk.Scrollbar(tree_frame, orient="vertical", command=self.tree.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.bind("<Double-1>", self._copy_selected_to_editor)

        list_buttons = ttk.Frame(main)
        list_buttons.grid(row=4, column=0, columnspan=3, sticky="w", pady=(2, 8))
        ttk.Button(list_buttons, text="Update selected", command=self._update_selected).pack(
            side="left"
        )
        ttk.Button(list_buttons, text="Remove selected", command=self._remove_selected).pack(
            side="left", padx=6
        )
        ttk.Button(list_buttons, text="Load JSON...", command=self._load_json).pack(side="left")
        ttk.Button(list_buttons, text="Export JSON...", command=self._export_json).pack(
            side="left", padx=(6, 0)
        )
        ttk.Button(list_buttons, text="Clear all", command=self._clear_all).pack(
            side="left", padx=6
        )

        options = ttk.Frame(main)
        options.grid(row=5, column=0, columnspan=3, sticky="ew", pady=4)
        ttk.Checkbutton(
            options,
            text="Modify source file (creates .bak backup)",
            variable=self.in_place_var,
            command=self._toggle_in_place,
        ).pack(side="left")
        ttk.Checkbutton(
            options, text="Use Word wildcards", variable=self.wildcards_var
        ).pack(side="left", padx=12)
        ttk.Checkbutton(
            options, text="Show Word while processing", variable=self.visible_var
        ).pack(side="left")

        self.progress_bar = ttk.Progressbar(
            main,
            variable=self.progress_var,
            maximum=100,
            mode="determinate",
        )
        self.progress_bar.grid(row=6, column=0, columnspan=3, sticky="ew", pady=(12, 4))

        footer = ttk.Frame(main)
        footer.grid(row=7, column=0, columnspan=3, sticky="ew", pady=(4, 0))
        footer.columnconfigure(0, weight=1)
        ttk.Label(footer, textvariable=self.status_var).grid(row=0, column=0, sticky="w")
        self.run_button = ttk.Button(footer, text="Replace text", command=self._start)
        self.run_button.grid(row=0, column=1, sticky="e")

    def _browse_input(self) -> None:
        path = filedialog.askopenfilename(
            title="Choose a Word document", filetypes=WORD_FILE_TYPES
        )
        if not path:
            return
        self.input_var.set(path)
        if not self.output_var.get() and not self.in_place_var.get():
            self.output_var.set(str(resolve_output(Path(path), None, False)))

    def _browse_output(self) -> None:
        input_path = Path(self.input_var.get()) if self.input_var.get() else Path("output.docx")
        path = filedialog.asksaveasfilename(
            title="Choose the output document",
            defaultextension=input_path.suffix or ".docx",
            initialfile=f"{input_path.stem}.replaced{input_path.suffix or '.docx'}",
            filetypes=WORD_FILE_TYPES,
        )
        if path:
            self.output_var.set(path)

    def _add_replacement(self) -> None:
        find, replacement = self._editor_values()
        if not find:
            messagebox.showerror("Missing text", "Enter the text to find.")
            return
        self.tree.insert("", "end", values=(find, replacement))
        self._clear_editor()

    def _copy_selected_to_editor(self, _event: object = None) -> None:
        selected = self._selected_item()
        if selected:
            find, replacement = self.tree.item(selected, "values")
            self.find_var.set(find)
            self.replace_var.set(replacement)

    def _update_selected(self) -> None:
        selected = self._selected_item()
        if not selected:
            messagebox.showinfo("Select a replacement", "Choose a replacement row first.")
            return
        find, replacement = self._editor_values()
        if not find:
            messagebox.showerror("Missing text", "Enter the text to find.")
            return
        self.tree.item(selected, values=(find, replacement))
        self._clear_editor()

    def _remove_selected(self) -> None:
        selected = self._selected_item()
        if selected:
            self.tree.delete(selected)

    def _editor_values(self) -> tuple[str, str]:
        return self.find_var.get(), self.replace_var.get()

    def _selected_item(self) -> str | None:
        selected = self.tree.selection()
        return selected[0] if selected else None

    def _clear_editor(self) -> None:
        self.find_var.set("")
        self.replace_var.set("")

    def _clear_all(self) -> None:
        for item in self.tree.get_children():
            self.tree.delete(item)

    def _load_json(self) -> None:
        path = filedialog.askopenfilename(
            title="Choose a replacements JSON file",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not path:
            return
        try:
            replacements = load_replacements([], Path(path))
        except ValueError as exc:
            messagebox.showerror("Invalid replacements file", str(exc))
            return
        for find, replacement in replacements.items():
            self.tree.insert("", "end", values=(find, replacement))
        self.status_var.set(f"Loaded {len(replacements)} replacement(s).")

    def _export_json(self) -> None:
        replacements = self._replacements()
        if not replacements:
            messagebox.showerror("Nothing to export", "Add at least one replacement first.")
            return
        path = filedialog.asksaveasfilename(
            title="Export replacements JSON",
            defaultextension=".json",
            initialfile="replacements.json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not path:
            return
        try:
            save_replacements(replacements, Path(path))
        except (OSError, ValueError) as exc:
            messagebox.showerror("Export failed", str(exc))
            return
        self.status_var.set(f"Exported {len(replacements)} replacement(s) to {path}.")
        messagebox.showinfo("Export complete", f"Saved:\n{path}")

    def _toggle_in_place(self) -> None:
        state = "disabled" if self.in_place_var.get() else "normal"
        self.output_entry.configure(state=state)
        self.output_button.configure(state=state)

    def _replacements(self) -> dict[str, str]:
        replacements = {}
        for item in self.tree.get_children():
            find, replacement = self.tree.item(item, "values")
            replacements[str(find)] = str(replacement)
        return replacements

    def _start(self) -> None:
        input_path = Path(self.input_var.get())
        output_text = self.output_var.get().strip()
        output_path = Path(output_text) if output_text else None
        replacements = self._replacements()
        in_place = self.in_place_var.get()
        wildcards = self.wildcards_var.get()
        visible = self.visible_var.get()

        try:
            if not input_path.is_file():
                raise ValueError("choose an existing input document")
            if input_path.suffix.lower() not in WORD_EXTENSIONS:
                raise ValueError("input must be a .doc, .docx, or .docm file")
            validate_text_only_replacements(replacements)
            resolved_output = resolve_output(input_path, output_path, in_place)
            if resolved_output.suffix.lower() not in WORD_EXTENSIONS:
                raise ValueError("output must be a .doc, .docx, or .docm file")
        except ValueError as exc:
            messagebox.showerror("Cannot start", str(exc))
            return

        self.run_button.configure(state="disabled")
        self.progress_var.set(0)
        self._last_progress_report = 0.0
        self.status_var.set("Processing document...")
        threading.Thread(
            target=self._run_worker,
            args=(input_path, resolved_output, replacements, in_place, wildcards, visible),
            daemon=True,
        ).start()

    def _run_worker(
        self,
        input_path: Path,
        output_path: Path,
        replacements: dict[str, str],
        in_place: bool,
        wildcards: bool,
        visible: bool,
    ) -> None:
        try:
            backup_path = None
            if in_place:
                backup_path = create_backup(input_path)
            replace_in_document(
                input_path,
                output_path,
                replacements,
                wildcards=wildcards,
                visible=visible,
                progress_callback=self._report_progress,
            )
        except Exception as exc:
            self.root.after(0, self._finish_error, str(exc))
            return
        self.root.after(0, self._finish_success, output_path, backup_path)

    def _report_progress(self, completed: int, total: int, find_text: str) -> None:
        now = time.monotonic()
        if completed != total and now - self._last_progress_report < 0.05:
            return
        self._last_progress_report = now
        self.root.after(0, self._update_progress, completed, total, find_text)

    def _update_progress(self, completed: int, total: int, find_text: str) -> None:
        percent = (completed / total * 100) if total else 100
        self.progress_var.set(percent)
        if completed and find_text:
            self.status_var.set(f"Processing {completed}/{total}: {find_text}")
        else:
            self.status_var.set(f"Processing 0/{total}...")

    def _finish_error(self, error: str) -> None:
        self.run_button.configure(state="normal")
        self.status_var.set("Replacement failed.")
        messagebox.showerror("Replacement failed", error)

    def _finish_success(self, output_path: Path, backup_path: Path | None) -> None:
        self.run_button.configure(state="normal")
        self.progress_var.set(100)
        self.status_var.set(f"Created: {output_path}")
        details = f"Created:\n{output_path}"
        if backup_path:
            details += f"\n\nBackup:\n{backup_path}"
        messagebox.showinfo("Replacement complete", details)


def main() -> None:
    root = tk.Tk()
    WordTextReplacerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
