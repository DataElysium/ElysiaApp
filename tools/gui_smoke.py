#!/usr/bin/env python3
"""A real graphics/DLL integration test. Requires an X11/Wayland/Windows desktop or virtual display."""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--bin',type=pathlib.Path,required=True)
parser.add_argument('--tool-library',type=pathlib.Path,help='Optional independently built UI DLL consuming the installed GUI SDK')
parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('/tmp/elysia-app-gui-smoke'))
args=parser.parse_args();source=args.bin.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
suffix='.exe' if os.name=='nt' else ''
with tempfile.TemporaryDirectory(prefix='elysia gui relocated ') as directory:
    work=pathlib.Path(directory);package=work/'app';package.mkdir()
    shutil.copytree(source/'sandbox',package/'sandbox')
    shutil.copy2(source/('elysia-app'+suffix),package/('elysia-app'+suffix))
    if args.tool_library:
        target=package/'sandbox/Mods/workbench/Assemblies'/args.tool_library.name
        shutil.copy2(args.tool_library,target)
    elsewhere=work/'elsewhere';elsewhere.mkdir()
    def execute(playset,extra,environment=None):
        return subprocess.run([str(package/('elysia-app'+suffix)),'--playset',str(package/'sandbox'/playset),'--',*extra],
                              cwd=elsewhere,env=environment,capture_output=True,text=True,timeout=40)
    for playset in ('gui.json','gui-simulation.json'):
        report=work/'report.json';capture=output/(playset+'.ppm')
        result=execute(playset,['--gui-hidden','--gui-frames','16','--gui-capture',str(capture),'--scope-report',str(report)])
        assert result.returncode==0,(result.returncode,result.stdout,result.stderr)
        assert capture.read_bytes().startswith(b'P6\n1440 900\n'),capture
        state=json.loads(report.read_text())
        assert state['context_alive'] and state['drawings']==16,state
        assert 'GUI frames=16' in result.stdout and 'vertices=0' not in result.stdout,result.stdout
        assert result.stdout.index('Workbench destroyed:')<result.stdout.index('GUI context destroyed'),result.stdout
        assert 'ticks=4' not in result.stdout,'GUI runner did not override the earlier headless runner'
        print('PASS',playset,': real OpenGL rendering, shared ImGui, DLL window + texture, retained context teardown')
    no_display={key:value for key,value in os.environ.items() if key not in ('DISPLAY','WAYLAND_DISPLAY')}
    result=execute('headless-last.json',[],no_display)
    assert result.returncode==0,(result.stdout,result.stderr)
    assert 'ticks=4' in result.stdout and 'GUI frames=' not in result.stdout,result.stdout
    assert 'context=absent' in result.stdout,result.stdout
    print('PASS headless-last: loading order overrides GUI runner without initializing graphics')
    # A domain error after GUI initialization must still preserve context for resource destructors.
    case=json.loads((package/'sandbox/Mods/case-loader/cases/default.json').read_text())
    case['scene']['prefabs']['demo::mover']['body'][0]['components']['Unknown']={}
    invalid=work/'invalid.json';invalid.write_text(json.dumps(case))
    report=work/'failure.json'
    result=execute('gui-simulation.json',['--case',str(invalid),'--gui-hidden','--gui-frames','2','--scope-report',str(report)])
    assert result.returncode!=0 and 'ElysiaApp:' in result.stderr,(result.stdout,result.stderr)
    assert json.loads(report.read_text())['context_alive'],result.stdout
    print('PASS failed startup: GUI context survives App teardown and error propagation')
    if args.tool_library:print('PASS independently built tool DLL / layered GUI SDK')
