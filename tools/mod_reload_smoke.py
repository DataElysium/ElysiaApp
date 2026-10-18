#!/usr/bin/env python3
"""Click the real Mods menu across three App lifetimes. Linux X11/XWayland only.
All playset edits stay in a relocated temporary package. Requires xdotool and libX11.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bin', type=Path, required=True)
args = parser.parse_args()
source = args.bin.resolve()
x11 = C.CDLL('libX11.so.6')
x11.XOpenDisplay.restype = C.c_void_p
x11.XOpenDisplay.argtypes = [C.c_char_p]
x11.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.c_void_p]
x11.XFlush.argtypes = [C.c_void_p]
x11.XCloseDisplay.argtypes = [C.c_void_p]
display = x11.XOpenDisplay(None)
assert display, 'An X11/XWayland display is required'

class Event(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong), ('root', C.c_ulong),
                ('subwindow', C.c_ulong), ('time', C.c_ulong), ('x', C.c_int), ('y', C.c_int),
                ('x_root', C.c_int), ('y_root', C.c_int), ('state', C.c_uint),
                ('button', C.c_uint), ('same_screen', C.c_int), ('padding', C.c_long * 10)]

class Crossing(C.Structure):
    _fields_ = Event._fields_[:12] + [('mode', C.c_int), ('detail', C.c_int),
                                   ('same_screen', C.c_int), ('focus', C.c_int),
                                   ('state', C.c_uint), ('padding', C.c_long * 10)]

try:
    with tempfile.TemporaryDirectory(prefix='elysia-mod-reload-') as directory:
        work = Path(directory)
        shutil.copytree(source / 'sandbox', work / 'sandbox')
        shutil.copy2(source / 'elysia-app', work / 'elysia-app')
        playset = work / 'sandbox/gui.json'
        original = playset.read_bytes()
        report, log = work / 'report.json', work / 'app.log'
        with log.open('w') as output:
            proc = subprocess.Popen([str(work / 'elysia-app'), '--playset', str(playset), '--',
                                     '--scope-report', str(report)], cwd=work,
                                    stdout=output, stderr=subprocess.STDOUT)
        try:
            def wait(predicate):
                for _ in range(150):
                    assert proc.poll() is None, log.read_text()
                    if predicate():
                        return
                    time.sleep(.1)
                raise AssertionError('Timed out: ' + log.read_text())

            def window():
                result = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(proc.pid)],
                                        capture_output=True, text=True)
                return int(result.stdout.splitlines()[0]) if result.stdout.strip() else None

            def click(x, y):
                win = window()
                subprocess.run(['xdotool', 'windowactivate', '--sync', str(win)], check=True)
                # EnterNotify prevents XWayland cursor polling from replacing synthetic motion.
                enter = Crossing(type=7, display=display, window=win, x=x, y=y, same_screen=1, focus=1)
                x11.XSendEvent(display, win, False, 16, C.byref(enter))
                x11.XFlush(display)
                time.sleep(.12)
                for kind, mask in [(6, 64), (4, 4), (5, 8)]:
                    event = Event(type=kind, display=display, window=win, x=x, y=y, button=1, same_screen=1)
                    x11.XSendEvent(display, win, False, mask, C.byref(event))
                    x11.XFlush(display)
                    time.sleep(.12)

            def manage():
                click(224, 15)
                click(290, 48)

            wait(window)
            time.sleep(.3)
            click(58, 160)  # Pause Workbench: next App must not inherit this state.
            manage()
            click(88, 353)  # GUI disabled while its dependent Workbench remains enabled.
            click(160, 166)  # Save must be rejected; same App and file remain.
            assert playset.read_bytes() == original and not report.exists()
            click(326, 166)  # Reset selection, also clears dependency error.
            click(88, 255)  # Disable Workbench only.
            click(160, 166)
            wait(lambda: log.read_text().count('GUI context destroyed') == 1 and window())
            assert json.loads(report.read_text())['paused']
            assert not json.loads(playset.read_text())['mods'][0]['enabled']
            assert 'libelysia-workbench.so' not in Path(f'/proc/{proc.pid}/maps').read_text()
            manage()
            click(88, 255)  # Enable Workbench again in the fresh App.
            click(160, 166)
            wait(lambda: log.read_text().count('GUI context destroyed') == 2 and window())
            assert 'libelysia-workbench.so' in Path(f'/proc/{proc.pid}/maps').read_text()
            click(106, 15)  # File -> Quit, without bypassing teardown.
            click(124, 48)
            assert proc.wait(timeout=10) == 0, log.read_text()
            text = log.read_text()
            assert text.count('GUI context destroyed') == 3, text
            assert text.count('Workbench destroyed:') == 2, text
            assert text.count('context=alive') == 2, text
            assert 'windows=0' in text and text.count('windows=2') == 2, text
            assert not json.loads(report.read_text())['paused'], 'Tool state survived App rebuild'
            print('PASS real Mods input, rejected dependency, save, disable/enable, DLL unload/reload, fresh state and three GUI teardowns')
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
finally:
    x11.XCloseDisplay(display)
