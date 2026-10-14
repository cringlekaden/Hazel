#pragma once
#ifdef HZ_PLATFORM_WINDOWS
#include <Windows.h>
#include <DbgHelp.h>
#include <cstdio>
// Test-only native diagnostics. Observe AVs on the probe thread without changing
// exception handling, Mono behavior, or production application dependencies.
struct WindowsCrashTrace {
    inline static HANDLE Process=nullptr;
    inline static DWORD Thread=0;
    inline static bool Enabled=false,Reporting=false;
    void* Handler=nullptr;
    static LONG CALLBACK Observe(EXCEPTION_POINTERS* info) {
        if(!Enabled || Reporting || GetCurrentThreadId()!=Thread ||
           info->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION)return EXCEPTION_CONTINUE_SEARCH;
        Reporting=true;
        std::fprintf(stderr,"Native access violation at %p (operation %llu, address %p)\n",info->ExceptionRecord->ExceptionAddress,
            static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[0]),
            reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
        auto context=*info->ContextRecord;STACKFRAME64 frame{};
        frame.AddrPC={context.Rip,0,AddrModeFlat};frame.AddrFrame={context.Rbp,0,AddrModeFlat};frame.AddrStack={context.Rsp,0,AddrModeFlat};
        for(int i=0;i<32 && frame.AddrPC.Offset;i++) {
            alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO)+512]{};
            auto* symbol=reinterpret_cast<SYMBOL_INFO*>(buffer);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=511;
            DWORD64 displacement=0;IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD lineOffset=0;
            if(SymFromAddr(Process,frame.AddrPC.Offset,&displacement,symbol)) {
                std::fprintf(stderr,"  %016llx %s + %llu",static_cast<unsigned long long>(frame.AddrPC.Offset),symbol->Name,static_cast<unsigned long long>(displacement));
                if(SymGetLineFromAddr64(Process,frame.AddrPC.Offset,&lineOffset,&line))std::fprintf(stderr," (%s:%lu)",line.FileName,line.LineNumber);
                std::fputc('\n',stderr);
            } else std::fprintf(stderr,"  %016llx\n",static_cast<unsigned long long>(frame.AddrPC.Offset));
            if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,Process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
        }
        std::fflush(stderr);Reporting=false;return EXCEPTION_CONTINUE_SEARCH;
    }
    WindowsCrashTrace() {
        Thread=GetCurrentThreadId();
        if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&Process,0,FALSE,DUPLICATE_SAME_ACCESS))return;
        SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS);
        if(SymInitialize(Process,nullptr,TRUE))Handler=AddVectoredExceptionHandler(1,Observe);
    }
    ~WindowsCrashTrace(){Enabled=false;if(Handler){RemoveVectoredExceptionHandler(Handler);SymCleanup(Process);}if(Process)CloseHandle(Process);Process=nullptr;}
};
#endif
