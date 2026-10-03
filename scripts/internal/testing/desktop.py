"""Deliver input only to a test process's owned native window; no production hooks."""
import ctypes as C
import os
import time

class Rect(C.Structure):
    _fields_=[('left',C.c_long),('top',C.c_long),('right',C.c_long),('bottom',C.c_long)]
class Point(C.Structure):
    _fields_=[('x',C.c_long),('y',C.c_long)]
class DisplayMode(C.Structure):
    # Win32 DEVMODEW's display union; fixed-width scalar fields preserve its ABI.
    _fields_=[('device',C.c_wchar*32),('spec',C.c_uint16),('driver',C.c_uint16),('size',C.c_uint16),('extra',C.c_uint16),('fields',C.c_uint32),
              ('position',C.c_int32*2),('orientation',C.c_uint32),('fixedOutput',C.c_uint32),
              ('color',C.c_int16),('duplex',C.c_int16),('yResolution',C.c_int16),('ttOption',C.c_int16),('collate',C.c_int16),('form',C.c_wchar*32),('logPixels',C.c_uint16),
              ('bits',C.c_uint32),('width',C.c_uint32),('height',C.c_uint32),('flags',C.c_uint32),('frequency',C.c_uint32),('icmMethod',C.c_uint32),('icmIntent',C.c_uint32),('media',C.c_uint32),('dither',C.c_uint32),('reserved1',C.c_uint32),('reserved2',C.c_uint32),('panningWidth',C.c_uint32),('panningHeight',C.c_uint32)]
class XError(C.Structure):
    _fields_=[('type',C.c_int),('display',C.c_void_p),('resource',C.c_ulong),('serial',C.c_ulong),('code',C.c_ubyte),('request',C.c_ubyte),('minor',C.c_ubyte)]
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
            self.user.GetCursorPos.argtypes=[C.POINTER(Point)];self.user.GetCursorPos.restype=C.c_bool
            self.user.SetProcessDpiAwarenessContext.argtypes=[C.c_void_p];self.user.SetProcessDpiAwarenessContext.restype=C.c_bool
            self.user.SetProcessDpiAwarenessContext(C.c_void_p(-4))
            self.user.EnumDisplaySettingsW.argtypes=[C.c_wchar_p,C.c_uint32,C.POINTER(DisplayMode)];self.user.EnumDisplaySettingsW.restype=C.c_bool
            self.user.ChangeDisplaySettingsW.argtypes=[C.POINTER(DisplayMode),C.c_uint32];self.user.ChangeDisplaySettingsW.restype=C.c_int32
            self.originalMode=None
            self.prepare_display()
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
        self.xerror=None
        self.error_callback=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(XError))
        @self.error_callback
        def handler(display,event):
            error=event.contents
            # Another process can destroy a window between tree enumeration and
            # property queries. These observations have no retained lifetime.
            if error.code not in (3,9):self.xerror=(error.code,error.request,error.minor)
            return 0
        self.error_handler=handler
        self.x.XSetErrorHandler.argtypes=[C.c_void_p];self.x.XSetErrorHandler.restype=C.c_void_p
        self.previous_error_handler=self.x.XSetErrorHandler(C.cast(handler,C.c_void_p))
        self.x.XSync.argtypes=[C.c_void_p,C.c_int];self.x.XSync.restype=C.c_int
    def atom(self,name):return self.x.XInternAtom(self.display,name.encode(),0)
    def prepare_display(self):
        if C.sizeof(DisplayMode)!=220:raise RuntimeError('Unexpected DEVMODEW ABI')
        current=DisplayMode();current.size=C.sizeof(current)
        if not self.user.EnumDisplaySettingsW(None,0xffffffff,C.byref(current)):raise C.WinError(C.get_last_error())
        if current.width>=1400 and current.height>=900:return
        candidates=[];index=0
        while True:
            mode=DisplayMode();mode.size=C.sizeof(mode)
            if not self.user.EnumDisplaySettingsW(None,index,C.byref(mode)):break
            if mode.width>=1400 and mode.height>=900 and mode.bits==current.bits:candidates.append(mode)
            index+=1
        if not candidates:raise RuntimeError('Windows desktop acceptance requires a supported display mode of at least 1400x900')
        selected=min(candidates,key=lambda mode:(mode.width*mode.height,-mode.frequency))
        result=self.user.ChangeDisplaySettingsW(C.byref(selected),0) # Dynamic only; never update the registry.
        if result:raise RuntimeError('Cannot establish isolated test desktop mode: '+str(result))
        self.originalMode=current
        print('Temporary acceptance desktop: '+str(selected.width)+'x'+str(selected.height),flush=True)
        time.sleep(.5)
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
        found=inspect(self.root);self.x.XSync(self.display,0)
        if self.xerror:raise RuntimeError('X11 desktop query failed: '+str(self.xerror))
        return found
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
            expected=(left+int(x),top+int(y))
            if not self.user.SetCursorPos(*expected):raise C.WinError(C.get_last_error())
            actual=Point()
            if not self.user.GetCursorPos(C.byref(actual)):raise C.WinError(C.get_last_error())
            print('Owned-window click: client='+str((int(x),int(y)))+', origin='+str((left,top))+', expected screen='+str(expected)+', actual='+str((actual.x,actual.y)),flush=True)
            if (actual.x,actual.y)!=expected:raise RuntimeError('Test cursor is clipped outside the owned-window control')
            self.user.PostMessageW(window,0x200,0,(int(y)<<16)|int(x));time.sleep(.1)
            self.user.PostMessageW(window,0x201,1,(int(y)<<16)|int(x));time.sleep(.25);self.user.PostMessageW(window,0x202,0,(int(y)<<16)|int(x))
        else:
            self.xt.XTestFakeMotionEvent(self.display,-1,left+int(x),top+int(y),0);self.x.XFlush(self.display);time.sleep(.1)
            self.xt.XTestFakeButtonEvent(self.display,1,1,0);self.x.XFlush(self.display);time.sleep(.25);self.xt.XTestFakeButtonEvent(self.display,1,0,0);self.x.XFlush(self.display)
        time.sleep(.3)
    def set_key(self,window,code,down):
        if os.name=='nt':
            scan=self.user.MapVirtualKeyW(code,0)
            self.user.PostMessageW(window,0x100 if down else 0x101,code,(scan<<16)|1|(0 if down else (1<<30)|(1<<31)))
        else:
            key=self.x.XKeysymToKeycode(self.display,code)
            self.xt.XTestFakeKeyEvent(self.display,key,1 if down else 0,0);self.x.XFlush(self.display)
    def key(self,window,code,seconds=.25):
        self.activate(window);self.set_key(window,code,True);time.sleep(seconds)
        self.set_key(window,code,False);time.sleep(.25)
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
        if os.name!='nt':
            self.x.XCloseDisplay(self.display);self.x.XSetErrorHandler(self.previous_error_handler)
        elif self.originalMode is not None:
            result=self.user.ChangeDisplaySettingsW(C.byref(self.originalMode),0)
            if result:raise RuntimeError('Cannot restore original test desktop mode: '+str(result))
            self.originalMode=None
    def capture(self,window,path=None):
        """Capture the real client framebuffer to PNG, using only OS APIs/stdlib."""
        import struct,zlib
        _,_,width,height=self.geometry(window)
        if os.name=='nt':
            class Header(C.Structure):
                _fields_=[('size',C.c_uint32),('width',C.c_int32),('height',C.c_int32),('planes',C.c_uint16),('bits',C.c_uint16),('compression',C.c_uint32),('imageSize',C.c_uint32),('x',C.c_int32),('y',C.c_int32),('used',C.c_uint32),('important',C.c_uint32)]
            gdi=C.WinDLL('gdi32',use_last_error=True)
            for name,result,args in [('CreateCompatibleDC',C.c_void_p,[C.c_void_p]),('CreateCompatibleBitmap',C.c_void_p,[C.c_void_p,C.c_int,C.c_int]),('SelectObject',C.c_void_p,[C.c_void_p,C.c_void_p]),('BitBlt',C.c_bool,[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_int,C.c_void_p,C.c_int,C.c_int,C.c_uint]),('GetDIBits',C.c_int,[C.c_void_p,C.c_void_p,C.c_uint,C.c_uint,C.c_void_p,C.c_void_p,C.c_uint]),('DeleteObject',C.c_bool,[C.c_void_p]),('DeleteDC',C.c_bool,[C.c_void_p])]:
                fn=getattr(gdi,name);fn.restype=result;fn.argtypes=args
            self.user.GetDC.argtypes=[C.c_void_p];self.user.GetDC.restype=C.c_void_p
            self.user.ReleaseDC.argtypes=[C.c_void_p,C.c_void_p];self.user.ReleaseDC.restype=C.c_int
            dc=self.user.GetDC(window);memory=gdi.CreateCompatibleDC(dc);bitmap=gdi.CreateCompatibleBitmap(dc,width,height)
            if not dc or not memory or not bitmap:raise RuntimeError('Cannot allocate screenshot')
            previous=gdi.SelectObject(memory,bitmap)
            try:
                if not gdi.BitBlt(memory,0,0,width,height,dc,0,0,0x00CC0020):raise C.WinError(C.get_last_error())
                gdi.SelectObject(memory,previous)
                header=Header();header.size=C.sizeof(header);header.width=width;header.height=-height;header.planes=1;header.bits=32
                buffer=C.create_string_buffer(width*height*4)
                if gdi.GetDIBits(dc,bitmap,0,height,buffer,C.byref(header),0)!=height:raise RuntimeError('Incomplete screenshot')
                data=buffer.raw
            finally:
                gdi.DeleteObject(bitmap);gdi.DeleteDC(memory);self.user.ReleaseDC(window,dc)
        else:
            class Image(C.Structure):
                _fields_=[('width',C.c_int),('height',C.c_int),('offset',C.c_int),('format',C.c_int),('data',C.c_void_p),('order',C.c_int),('unit',C.c_int),('bitOrder',C.c_int),('pad',C.c_int),('depth',C.c_int),('stride',C.c_int),('bits',C.c_int),('red',C.c_ulong),('green',C.c_ulong),('blue',C.c_ulong)]
            self.x.XGetImage.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_int,C.c_uint,C.c_uint,C.c_ulong,C.c_int];self.x.XGetImage.restype=C.POINTER(Image)
            self.x.XDestroyImage.argtypes=[C.POINTER(Image)];self.x.XDestroyImage.restype=C.c_int
            image=self.x.XGetImage(self.display,window,0,0,width,height,C.c_ulong(-1),2)
            if not image:raise RuntimeError('Cannot capture owned X11 window')
            try:
                meta=image.contents
                if meta.bits!=32 or meta.order!=0 or (meta.red,meta.green,meta.blue)!=(0xff0000,0xff00,0xff):raise RuntimeError('Unsupported screenshot pixel format')
                raw=C.string_at(meta.data,meta.stride*height)
                data=b''.join(raw[y*meta.stride:y*meta.stride+width*4] for y in range(height))
            finally:self.x.XDestroyImage(image)
        rows=[]
        for y in range(height):
            row=data[y*width*4:(y+1)*width*4];rgb=bytearray(width*3)
            rgb[0::3]=row[2::4];rgb[1::3]=row[1::4];rgb[2::3]=row[0::4];rows.append(b'\0'+rgb)
        def chunk(kind,body):return struct.pack('>I',len(body))+kind+body+struct.pack('>I',zlib.crc32(kind+body))
        if path is None:return width,height,b''.join(row[1:] for row in rows)
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows),6))+chunk(b'IEND',b''))
