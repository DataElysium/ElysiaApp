#!/usr/bin/env python3
"""Run the packaged host from an unrelated CWD; compare static/DLL outcomes and error exits."""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--bin',type=pathlib.Path,required=True,help='Build directory containing host executables and sandbox/')
parser.add_argument('--external',type=pathlib.Path,help='Optional separately built executable linked to the installed SDK')
args=parser.parse_args()
source=args.bin.resolve()
suffix='.exe' if (source/'elysia-app.exe').exists() else ''
with tempfile.TemporaryDirectory(prefix='elysia app relocated ') as tmp:
    work=pathlib.Path(tmp)
    package=work/('relocated app' if os.name=='nt' else '移动的应用') # Spaces; UTF-8 paths additionally exercised on POSIX.
    package.mkdir()
    shutil.copytree(source/'sandbox',package/'sandbox')
    for name in ('elysia-app','elysia-app-static'):
        shutil.copy2(source/(name+suffix),package/(name+suffix))
    elsewhere=work/'unrelated';elsewhere.mkdir()
    def run(executable,playset,*plugin_args,ok=True):
        result=subprocess.run([str(executable),'--playset',str(playset),*plugin_args],cwd=elsewhere,capture_output=True,text=True)
        assert (result.returncode==0)==ok,(result.returncode,result.stdout,result.stderr)
        return result
    dynamic=run(package/('elysia-app'+suffix),package/'sandbox/dynamic.json')
    mixed=run(package/('elysia-app-static'+suffix),package/'sandbox/mixed.json')
    expected=['Alpha x=8','Beta x=22','ticks=4']
    assert sorted(dynamic.stdout.splitlines())==expected,dynamic.stdout
    assert dynamic.stdout==mixed.stdout,(dynamic.stdout,mixed.stdout)
    if args.external:
        independent=run(args.external.resolve(),package/'sandbox/dynamic.json')
        assert independent.stdout==dynamic.stdout,independent.stdout
    override=json.loads((package/'sandbox/Mods/case-loader/cases/default.json').read_text())
    override['steps']=7
    override_file=work/'override.json';override_file.write_text(json.dumps(override))
    result=run(package/('elysia-app'+suffix),package/'sandbox/dynamic.json','--','--case',str(override_file))
    assert sorted(result.stdout.splitlines())==['Alpha x=14','Beta x=31','ticks=7'],result.stdout
    # Broken business input must fail through a host-owned error, even after Runtime destroys its DLLs.
    override['scene']['prefabs']['demo::mover']['body'][0]['components']['Unknown']={}
    override_file.write_text(json.dumps(override))
    result=run(package/('elysia-app'+suffix),package/'sandbox/dynamic.json','--','--case',str(override_file),ok=False)
    assert 'ElysiaApp:' in result.stderr and 'App runner:' in result.stderr,result.stderr
    listing=subprocess.run([str(package/('elysia-app'+suffix)),'--list',str(package/'sandbox/Mods')],cwd=elsewhere,capture_output=True,text=True,check=True)
    assert 'demo.case_loader' in listing.stdout,listing.stdout
    print('PASS relocated package, foreign CWD, static/DLL parity, plugin case arguments and clean failure exit'
          + ('; installed-SDK external consumer' if args.external else ''))
