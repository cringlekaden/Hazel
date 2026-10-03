"""Deliver input only to a test process's owned native window; no production hooks."""
import ctypes as C
import os
import time

class Rect(C.Structure):
    _fields_=[('left',C.c_long),('top',C.c_long),('right',C.c_long),('bottom',C.c_long)]
class Point(C.Structure):
    _fields_=[('x',C.c_long),('y',C.c_long)]
class Data(C.Union):
    _fields_=[('b',C.c_char*20),('s',C.c_short*10),('l',C.c_long*5)]
class Message(C.Structure):
    _fields_=[('type',C.c_int),('serial',C.c_ulong),('send',C.c_int),('display',C.c_void_p),('window',C.c_ulong),('message',C.c_ulong),('format',C.c_int),('data',Data)]
class Event(C.Union):
    _fields_=[('message',Message),('pad',C.c_long*24)]

class Desktop:
    def __init__(self):
        if os.name=='nt':
            self.user=C.WinDLL('user32',use_last_error=True)
            self.callback=C.WINFUNCTYPE(C.c_bool,C.c_void_p,C.c_ssize_t)
            signatures=[('EnumWindows',C.c_bool,[self.callback,C.c_ssize_t]),('GetWindowThreadProcessId',C.c_ulong,[C.c_void_p,C.POINTER(C.c_ulong)]),('GetWindowTextW',C.c_int,[C.c_void_p,C.c_wchar_p,C.c_int]),('GetClientRect',C.c_bool,[C.c_void_p,C.POINTER(Rect)]),('ClientToScreen',C.c_bool,[C.c_void_p,C.POINTER(Point)]),('PostMessageW',C.c_bool,[C.c_void_p,C.c_uint,C.c_size_t,C.c_ssize_t]),('SetWindowPos',C.c_bool,[C.c_void_p,C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_int,C.c_uint]),('GetWindowRect',C.c_bool,[C.c_void_p,C.POINTER(Rect)]),('SetForegroundWindow',C.c_bool,[C.c_void_p]),('SetCursorPos',C.c_bool,[C.c_int,C.c_int]),('MapVirtualKeyW',C.c_uint,[C.c_uint,C.c_uint])]
            for name,result,args in signatures:fn=getattr(self.user,name);fn.restype=result;fn.argtypes=args
            return
        self.x=C.CDLL('libX11.so.6');self.xt=C.CDLL('libXtst.so.6')
        signatures=[('XOpenDisplay',C.c_void_p,[C.c_char_p]),('XDefaultRootWindow',C.c_ulong,[C.c_void_p]),('XQueryTree',C.c_int,[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)]),('XFetchName',C.c_int,[C.c_void_p,C.c_ulong,C.POINTER(C.c_void_p)]),('XInternAtom',C.c_ulong,[C.c_void_p,C.c_char_p,C.c_int]),('XGetWindowProperty',C.c_int,[C.c_void_p,C.c_ulong,C.c_ulong,C.c_long,C.c_long,C.c_int,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_int),C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.c_void_p)]),('XFree',C.c_int,[C.c_void_p]),('XFlush',C.c_int,[C.c_void_p]),('XSetInputFocus',C.c_int,[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]),('XRaiseWindow',C.c_int,[C.c_void_p,C.c_ulong]),('XTranslateCoordinates',C.c_int,[C.c_void_p,C.c_ulong,C.c_ulong,C.c_int,C.c_int,C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_ulong)]),('XGetGeometry',C.c_int,[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint)]),('XResizeWindow',C.c_int,[C.c_void_p,C.c_ulong,C.c_uint,C.c_uint]),('XSendEvent',C.c_int,[C.c_void_p,C.c_ulong,C.c_int,C.c_long,C.POINTER(Event)]),('XKeysymToKeycode',C.c_ubyte,[C.c_void_p,C.c_ulong]),('XCloseDisplay',C.c_int,[C.c_void_p])]
        for name,result,args in signatures:fn=getattr(self.x,name);fn.restype=result;fn.argtypes=args
        self.xt.XTestFakeMotionEvent.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_ulong]
        self.xt.XTestFakeButtonEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
        self.xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
        self.display=self.x.XOpenDisplay(None)
        if not self.display:raise RuntimeError('X11 display required for desktop acceptance')
        self.root=self.x.XDefaultRootWindow(self.display)
    def atom(self,name):return self.x.XInternAtom(self.display,name.encode(),0)
    def find(self,pid,title):
        if os.name=='nt':
            found=[]
            @self.callback
            def inspect(window,unused):
                owner=C.c_ulong();self.user.GetWindowThreadProcessId(window,C.byref(owner));name=C.create_unicode_buffer(256);self.user.GetWindowTextW(window,name,256)
                if owner.value==pid and name.value==title:found.append(window)
                return True
            self.user.EnumWindows(inspect,0);return found[0] if found else None
        def inspect(window,depth=0):
            name=C.c_void_p();self.x.XFetchName(self.display,window,C.byref(name))
            try:matches=name and C.string_at(name).decode(errors='replace')==title
            finally:
                if name:self.x.XFree(name)
            if matches:
                kind=C.c_ulong();fmt=C.c_int();count=C.c_ulong();remaining=C.c_ulong();data=C.c_void_p()
                self.x.XGetWindowProperty(self.display,window,self.atom('_NET_WM_PID'),0,1,0,0,C.byref(kind),C.byref(fmt),C.byref(count),C.byref(remaining),C.byref(data))
                try:
                    if data and count.value and C.cast(data,C.POINTER(C.c_ulong))[0]==pid:return window
                finally:
                    if data:self.x.XFree(data)
            if depth>8:return None
            root=C.c_ulong();parent=C.c_ulong();children=C.POINTER(C.c_ulong)();count=C.c_uint()
            self.x.XQueryTree(self.display,window,C.byref(root),C.byref(parent),C.byref(children),C.byref(count))
            try:
                for child in children[:count.value]:
                    found=inspect(child,depth+1)
                    if found:return found
            finally:
                if children:self.x.XFree(children)
        return inspect(self.root)
    def geometry(self,window):
        if os.name=='nt':
            rect=Rect();point=Point();self.user.GetClientRect(window,C.byref(rect));self.user.ClientToScreen(window,C.byref(point));return point.x,point.y,rect.right,rect.bottom
        root=C.c_ulong();x=C.c_int();y=C.c_int();w=C.c_uint();h=C.c_uint();border=C.c_uint();depth=C.c_uint();child=C.c_ulong()
        self.x.XGetGeometry(self.display,window,C.byref(root),C.byref(x),C.byref(y),C.byref(w),C.byref(h),C.byref(border),C.byref(depth))
        self.x.XTranslateCoordinates(self.display,window,self.root,0,0,C.byref(x),C.byref(y),C.byref(child));return x.value,y.value,w.value,h.value
    def activate(self,window):
        if os.name=='nt':self.user.SetForegroundWindow(window)
        else:self.x.XRaiseWindow(self.display,window);self.x.XSetInputFocus(self.display,window,2,0);self.x.XFlush(self.display)
        time.sleep(.2)
    def click(self,window,x,y):
        self.activate(window);left,top,_,_=self.geometry(window)
        if os.name=='nt':
            self.user.SetCursorPos(left+int(x),top+int(y));self.user.PostMessageW(window,0x200,0,(int(y)<<16)|int(x));time.sleep(.1)
            self.user.PostMessageW(window,0x201,1,(int(y)<<16)|int(x));time.sleep(.25);self.user.PostMessageW(window,0x202,0,(int(y)<<16)|int(x))
        else:
            self.xt.XTestFakeMotionEvent(self.display,-1,left+int(x),top+int(y),0);self.x.XFlush(self.display);time.sleep(.1)
            self.xt.XTestFakeButtonEvent(self.display,1,1,0);self.x.XFlush(self.display);time.sleep(.25);self.xt.XTestFakeButtonEvent(self.display,1,0,0);self.x.XFlush(self.display)
        time.sleep(.3)
    def key(self,window,code,seconds=.25):
        self.activate(window)
        if os.name=='nt':
            scan=self.user.MapVirtualKeyW(code,0);self.user.PostMessageW(window,0x100,code,(scan<<16)|1);time.sleep(seconds);self.user.PostMessageW(window,0x101,code,(scan<<16)|(1<<30)|(1<<31)|1)
        else:
            key=self.x.XKeysymToKeycode(self.display,code);self.xt.XTestFakeKeyEvent(self.display,key,1,0);self.x.XFlush(self.display);time.sleep(seconds);self.xt.XTestFakeKeyEvent(self.display,key,0,0);self.x.XFlush(self.display)
        time.sleep(.25)
    def resize(self,window,width,height):
        if os.name=='nt':
            rect=Rect();self.user.GetWindowRect(window,C.byref(rect));_,_,w,h=self.geometry(window);self.user.SetWindowPos(window,None,0,0,width+rect.right-rect.left-w,height+rect.bottom-rect.top-h,6)
        else:self.x.XResizeWindow(self.display,window,width,height);self.x.XFlush(self.display)
        time.sleep(.4)
    def close(self,window):
        if os.name=='nt':self.user.PostMessageW(window,0x10,0,0)
        else:
            event=Event();message=event.message;message.type=33;message.send=1;message.display=self.display;message.window=window;message.message=self.atom('WM_PROTOCOLS');message.format=32;message.data.l[0]=self.atom('WM_DELETE_WINDOW');self.x.XSendEvent(self.display,window,0,0,C.byref(event));self.x.XFlush(self.display)
    def shutdown(self):
        if os.name!='nt':self.x.XCloseDisplay(self.display)
