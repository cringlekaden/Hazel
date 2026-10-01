#!/usr/bin/env python3
"""X11 smoke runner: copy assets, request WM_DELETE_WINDOW, require clean exit."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

class MessageData(C.Union):
    _fields_ = [('b', C.c_char * 20), ('s', C.c_short * 10), ('l', C.c_long * 5)]
class ClientMessage(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong), ('message_type', C.c_ulong),
                ('format', C.c_int), ('data', MessageData)]
class Event(C.Union):
    _fields_ = [('client', ClientMessage), ('pad', C.c_long * 24)]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--assets', type=Path, default=Path('Sandbox'))
    parser.add_argument('--require-order', action='store_true')
    parser.add_argument('--editor', action='store_true', help='Exercise Hazelnut with an isolated Unicode project argument')
    args=parser.parse_args()
    x=C.CDLL('libX11.so.6')
    for name, restype, argtypes in [
        ('XOpenDisplay',C.c_void_p,[C.c_char_p]),
        ('XDefaultRootWindow',C.c_ulong,[C.c_void_p]),
        ('XQueryTree',C.c_int,[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)]),
        ('XFetchName',C.c_int,[C.c_void_p,C.c_ulong,C.POINTER(C.c_void_p)]),
        ('XInternAtom',C.c_ulong,[C.c_void_p,C.c_char_p,C.c_int]),
        ('XGetWindowProperty',C.c_int,[C.c_void_p,C.c_ulong,C.c_ulong,C.c_long,C.c_long,C.c_int,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_int),C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.c_void_p)]),
        ('XSendEvent',C.c_int,[C.c_void_p,C.c_ulong,C.c_int,C.c_long,C.POINTER(Event)]),
        ('XFlush',C.c_int,[C.c_void_p]), ('XFree',C.c_int,[C.c_void_p]),
        ('XCloseDisplay',C.c_int,[C.c_void_p])]:
        f=getattr(x,name); f.restype=restype; f.argtypes=argtypes
    display=x.XOpenDisplay(None)
    if not display: raise RuntimeError('Cannot open X11 display')
    pid_atom=x.XInternAtom(display,b'_NET_WM_PID',0)
    def owned_by(window,pid):
        kind=C.c_ulong(); fmt=C.c_int(); count=C.c_ulong(); remaining=C.c_ulong(); data=C.c_void_p()
        status=x.XGetWindowProperty(display,window,pid_atom,0,1,0,6,C.byref(kind),C.byref(fmt),C.byref(count),C.byref(remaining),C.byref(data))
        try:
            return status==0 and kind.value==6 and fmt.value==32 and count.value==1 and bool(data) and C.cast(data,C.POINTER(C.c_ulong))[0]==pid
        finally:
            if data: x.XFree(data)
    def find(window,title,pid):
        name=C.c_void_p()
        if x.XFetchName(display,window,C.byref(name)) and name:
            found=C.string_at(name).decode(errors='replace')==title
            x.XFree(name)
            if found and owned_by(window,pid): return window
        root,parent=C.c_ulong(),C.c_ulong()
        children=C.POINTER(C.c_ulong)(); count=C.c_uint()
        if x.XQueryTree(display,window,C.byref(root),C.byref(parent),C.byref(children),C.byref(count)):
            ids=[children[i] for i in range(count.value)]
            if children: x.XFree(children)
            for child in ids:
                result=find(child,title,pid)
                if result: return result
        return None
    with tempfile.TemporaryDirectory(prefix='hazel-graceful-') as directory:
        run=Path(directory)
        shutil.copytree(args.assets/'assets',run/'assets')
        command=[str(args.executable.resolve())]
        title='Hazelnut' if args.editor else 'Hazel Engine'
        if args.editor:
            shutil.copytree(args.assets/'Resources',run/'Resources')
            shutil.copytree(args.assets/'SandboxProject',run/'SandboxProject')
            project=run/'SandboxProject/project-é-🚀.hproj'
            shutil.copyfile(run/'SandboxProject/Sandbox.hproj',project)
            command.append(str(project))
        if (args.assets/'imgui.ini').exists(): shutil.copyfile(args.assets/'imgui.ini',run/'imgui.ini')
        with (run/'runtime.log').open('w') as log:
            process=subprocess.Popen(command,cwd=run,stdout=log,stderr=subprocess.STDOUT)
            try:
                deadline=time.monotonic()+20
                window=None
                while not window and time.monotonic()<deadline and process.poll() is None:
                    window=find(x.XDefaultRootWindow(display),title,process.pid)
                    time.sleep(0.1)
                if not window: raise RuntimeError(f'{title} window did not appear')
                time.sleep(2)
                event=Event(); event.client.type=33; event.client.send_event=1
                event.client.display=display; event.client.window=window
                event.client.message_type=x.XInternAtom(display,b'WM_PROTOCOLS',0)
                event.client.format=32
                event.client.data.l[0]=x.XInternAtom(display,b'WM_DELETE_WINDOW',0)
                event.client.data.l[1]=0
                if not x.XSendEvent(display,window,0,0,C.byref(event)): raise RuntimeError('Close request failed')
                x.XFlush(display)
                result=process.wait(timeout=20)
                print((run/'runtime.log').read_text())
                if result: raise RuntimeError(f'{title} exited {result}')
                shutdown=json.loads((run/'HazelProfile-Shutdown.json').read_text())
                names=[item['name'] for item in shutdown['traceEvents']]
                print('Shutdown trace functions:',json.dumps(names))
                if args.require_order:
                    renderer=next(i for i,n in enumerate(names) if 'Renderer2D::Shutdown' in n)
                    layer=next(i for i,n in enumerate(names) if ('EditorLayer::OnDetach' if args.editor else 'Sandbox2D::OnDetach') in n)
                    imgui=next(i for i,n in enumerate(names) if 'ImGuiLayer::OnDetach' in n)
                    window=next(i for i,n in enumerate(names) if 'LinuxWindow::Shutdown' in n)
                    if not layer < imgui < renderer < window:
                        raise RuntimeError('Unexpected layer/ImGui/renderer/window shutdown order')
                    print('PASS: user layer and ImGui detach before renderer shutdown; renderer before native window')
                print(f'PASS: WM_DELETE_WINDOW delivered; {title} returned 0 and shutdown trace is valid JSON')
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
    x.XCloseDisplay(display)

if __name__=='__main__': main()
