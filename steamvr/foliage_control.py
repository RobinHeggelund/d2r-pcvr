"""Desktop editor for the live [foliage] INI controls. Python + Tk, no packages.

The game stays in VR mode. Apply edits only the selected profile's foliage
keys; the plugin's existing half-second INI poll supplies them to the shader.
"""
from pathlib import Path
import argparse
import codecs
import math
import os
import re
import tempfile

FIELDS = (
    ('footprint', 'Placement footprint', 'Allowed square. Increase spacing too to reach farther.'),
    ('origin_x', 'Placement origin X', '1x is neutral. Each 1x adds 90 world units along ground X.'),
    ('origin_z', 'Placement origin Z', '1x is neutral. Each 1x adds 90 world units along ground Z.'),
    ('spacing', 'Candidate spacing', 'Spreads the fixed candidate count. Wider means less dense.'),
    ('camera_acceptance', 'Camera acceptance', 'Multiplier of native projected camera bounds.'),
    ('height_scale', 'Heightmap sampling scale', '1x is native. Does not expand the underlying terrain data.'),
    ('height_offset_x', 'Heightmap offset X', '1x is neutral. Each 1x adds half a texture width.'),
    ('height_offset_z', 'Heightmap offset Z', '1x is neutral. Each 1x adds half a texture width.'),
)
DEFAULTS = {key: 1.0 for key, _, _ in FIELDS}
CENTERED = dict(DEFAULTS, footprint=2, spacing=2, camera_acceptance=2,
                height_scale=.5, height_offset_x=1.5, height_offset_z=1.5)
SECTION = re.compile(r'^\s*\[([^\]]+)\]\s*(?:;[^\r\n]*)?[\r\n]*$')
KEY = re.compile(r'^(\s*([^=;#\r\n]+?)\s*=\s*)([^\r\n]*)(\r?\n)?$')


def normalize(value):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError('Values must be finite numbers.')
    return math.floor(min(10, max(0, value)) * 20 + .5) / 20


def document(raw):
    if raw.startswith(codecs.BOM_UTF16_LE):
        return raw.decode('utf-16'), 'utf-16'
    if raw.startswith(codecs.BOM_UTF16_BE):
        return raw[2:].decode('utf-16-be'), 'utf-16-be-bom'
    if raw.startswith(codecs.BOM_UTF8):
        return raw.decode('utf-8-sig'), 'utf-8-sig'
    try:
        return raw.decode('utf-8'), 'utf-8'
    except UnicodeDecodeError:
        return raw.decode('latin-1'), 'latin-1'


def scan(text):
    """Locate only foliage keys, keeping unrelated INI text opaque."""
    lines = text.splitlines(keepends=True)
    start = end = None
    found = {}
    active = False
    for i, line in enumerate(lines):
        section = SECTION.match(line)
        if section:
            if active:
                end = i
            active = section[1].strip().lower() == 'foliage'
            if active:
                if start is not None:
                    raise ValueError('Duplicate [foliage] sections. Resolve them before editing.')
                start = i
        elif active and (entry := KEY.match(line)):
            key = entry[2].strip().lower()
            if key in DEFAULTS:
                if key in found:
                    raise ValueError(f'Duplicate foliage key: {key}')
                found[key] = (i, entry)
    if start is not None and end is None:
        end = len(lines)
    return lines, start, end, found


def values_from(raw):
    text, _ = document(raw)
    _, _, _, found = scan(text)
    values = dict(DEFAULTS)
    for key, (_, entry) in found.items():
        number = entry[3].split(';', 1)[0].strip().strip('"')
        try:
            values[key] = normalize(number)
        except ValueError:
            values[key] = 1.0
    return values


def read_values(path):
    return values_from(Path(path).read_bytes())


def save_values(path, values, expected):
    path = Path(path)
    raw = path.read_bytes()  # Existing profiles only; never create a game profile.
    if values_from(raw) != expected:
        raise ValueError('Foliage settings changed elsewhere. Reload before applying.')
    values = {key: normalize(values[key]) for key in DEFAULTS}
    text, encoding = document(raw)
    lines, start, end, found = scan(text)
    newline = '\r\n' if '\r\n' in text else '\n'
    for key, (i, entry) in found.items():
        comment = entry[3].partition(';')
        suffix = (' ;' + comment[2]) if comment[1] else ''
        lines[i] = f'{entry[1]}{values[key]:.2f}{suffix}{entry[4] or ""}'
    additions = [f'{key}={values[key]:.2f}{newline}' for key in DEFAULTS if key not in found]
    if start is None:
        if lines and not lines[-1].endswith(('\n', '\r')):
            lines[-1] += newline
        lines.extend([newline, f'[foliage]{newline}', *additions])
    else:
        if additions and end and not lines[end - 1].endswith(('\n', '\r')):
            lines[end - 1] += newline
        lines[end:end] = additions
    result = ''.join(lines)
    encoded = (codecs.BOM_UTF16_BE + result.encode('utf-16-be')
               if encoding == 'utf-16-be-bom' else result.encode(encoding))
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + '.foliage-', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'wb') as output:
            output.write(encoded)
            output.flush()
            os.fsync(output.fileno())
        if path.read_bytes() != raw:
            raise ValueError('The INI changed during Apply. Reload and try again.')
        # Preserve the first original file, matching the VR menu's backup policy.
        backup = path.with_name(path.name + '.before-foliage-controls')
        try:
            with backup.open('xb') as output:
                output.write(raw)
        except FileExistsError:
            pass
        os.replace(temporary, path)
        if os.name == 'nt':
            # Invalidate the Windows profile API cache used by the game plugin.
            import ctypes
            flush = ctypes.WinDLL('kernel32', use_last_error=True).WritePrivateProfileStringW
            flush.argtypes = (ctypes.c_wchar_p,) * 4
            flush.restype = ctypes.c_int
            flush(None, None, None, str(path.resolve()))
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    return values


class Editor:
    def __init__(self, root, game, ini=None):
        import tkinter as tk
        from tkinter import ttk
        self.root, self.game, self.base = root, Path(game), None
        self.path = tk.StringVar(value=str(ini or self.profile_path('pcvr-dev')))
        self.profile = tk.StringVar(value='Custom' if ini else 'pcvr-dev')
        self.status = tk.StringVar()
        self.controls = {key: tk.StringVar(value='1.00') for key in DEFAULTS}
        root.title('Foliage controls')
        root.geometry('880x740')
        root.minsize(780, 700)
        panel = ttk.Frame(root, padding=18)
        panel.pack(fill='both', expand=True)
        panel.columnconfigure(1, weight=1)
        ttk.Label(panel, text='Foliage controls', font=('Segoe UI', 18, 'bold')).grid(row=0, column=0, columnspan=3, sticky='w')
        ttk.Label(panel, text='Edit from the desktop while the game stays in VR. Apply updates the running game.').grid(row=1, column=0, columnspan=3, sticky='w', pady=(4, 14))
        ttk.Label(panel, text='Profile').grid(row=2, column=0, sticky='w')
        choice = ttk.Combobox(panel, textvariable=self.profile, values=('pcvr-dev', 'D2R-PCVR', 'Custom'), state='readonly')
        choice.grid(row=2, column=1, sticky='ew', padx=10)
        choice.bind('<<ComboboxSelected>>', self.select_profile)
        ttk.Button(panel, text='Browse INI...', command=self.browse).grid(row=2, column=2)
        ttk.Label(panel, textvariable=self.path, wraplength=800).grid(row=3, column=0, columnspan=3, sticky='w', pady=(5, 14))
        self.scales = {}
        for index, (key, label, detail) in enumerate(FIELDS):
            row = 4 + index * 2
            ttk.Label(panel, text=label).grid(row=row, column=0, sticky='w')
            scale = tk.Scale(panel, from_=0, to=10, resolution=.05, orient='horizontal',
                             variable=self.controls[key], showvalue=False, highlightthickness=0)
            scale.grid(row=row, column=1, sticky='ew', padx=10)
            self.scales[key] = scale
            ttk.Spinbox(panel, from_=0, to=10, increment=.05, width=7, textvariable=self.controls[key]).grid(row=row, column=2)
            ttk.Label(panel, text=detail, foreground='#555555').grid(row=row + 1, column=0, columnspan=3, sticky='w', pady=(0, 9))
        buttons = ttk.Frame(panel)
        buttons.grid(row=20, column=0, columnspan=3, sticky='ew', pady=(8, 12))
        ttk.Button(buttons, text='Native layout', command=lambda: self.preset(DEFAULTS)).pack(side='left')
        ttk.Button(buttons, text='Centered 360', command=lambda: self.preset(CENTERED)).pack(side='left', padx=8)
        ttk.Button(buttons, text='Reload INI', command=self.reload).pack(side='right')
        ttk.Button(buttons, text='Apply to game', command=self.apply).pack(side='right', padx=8)
        ttk.Label(panel, textvariable=self.status, wraplength=800).grid(row=21, column=0, columnspan=3, sticky='w')
        ttk.Label(panel, text='0–10x in 0.05x steps. Presets change placement, not the startup candidate budget.\nNext investigation: Heightmap generation and world coverage.', foreground='#555555').grid(row=22, column=0, columnspan=3, sticky='w', pady=(12, 0))
        self.reload()

    def profile_path(self, profile):
        return self.game / 'mods' / profile / 'd2rloader/plugins/d2r_pcvr.ini'

    def select_profile(self, _=None):
        if self.profile.get() == 'Custom':
            self.browse()
        else:
            self.path.set(str(self.profile_path(self.profile.get())))
            self.reload()

    def browse(self):
        from tkinter import filedialog
        selected = filedialog.askopenfilename(title='Select the active profile INI', filetypes=[('INI files', '*.ini')], initialdir=self.game)
        if selected:
            self.profile.set('Custom')
            self.path.set(selected)
            self.reload()

    def preset(self, values):
        for key, value in values.items():
            self.controls[key].set(f'{value:.2f}')
        self.status.set('Draft updated. Press Apply to game to save these values.')

    def reload(self):
        self.base = None
        try:
            values = read_values(self.path.get())
            self.preset(values)
            self.base = values
            self.status.set('Loaded. Adjust values, then Apply to game.')
        except (OSError, ValueError) as error:
            self.status.set(f'Cannot load INI: {error}. Select the profile currently running in the game.')

    def apply(self):
        if self.base is None:
            self.status.set('Select and load an existing profile INI first.')
            return
        try:
            values = {key: variable.get() for key, variable in self.controls.items()}
            saved = save_values(self.path.get(), values, self.base)
            self.preset(saved)
            self.base = saved
            self.status.set('Saved. The game reads this INI about twice a second; no restart needed.')
        except (OSError, ValueError) as error:
            self.status.set(f'Not applied: {error}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=Path('E:/Games/Diablo II Resurrected'))
    parser.add_argument('--ini', type=Path, help='Explicit profile INI instead of the default pcvr-dev profile')
    args = parser.parse_args()
    import tkinter as tk
    root = tk.Tk()
    Editor(root, args.game, args.ini)
    root.mainloop()


if __name__ == '__main__':
    main()
