#include "core.hpp"
#include <iostream>
#include <bcrypt.h>
using namespace apcs;
static std::string hash(const std::string& s){
    BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;DWORD sz=0,n=0;unsigned char out[32];
    if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider failed");
    BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,(PUCHAR)&sz,sizeof sz,&n,0);std::vector<unsigned char> obj(sz);
    if(BCryptCreateHash(a,&h,obj.data(),sz,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(a,0);throw std::runtime_error("SHA256 failed");}
    BCryptHashData(h,(PUCHAR)s.data(),(ULONG)s.size(),0);BCryptFinishHash(h,out,32,0);BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);
    std::ostringstream o;for(auto c:out)o<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)c;return o.str();
}
static int run(const std::vector<std::wstring>& args,std::string& output){
    SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};HANDLE rd=nullptr,wr=nullptr;
    if(!CreatePipe(&rd,&wr,&sa,0))throw std::runtime_error("CreatePipe failed");
    SetHandleInformation(rd,HANDLE_FLAG_INHERIT,0);
    STARTUPINFOW si={};si.cb=sizeof si;si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=wr;si.hStdError=wr;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    std::wstring cmd;for(auto& arg:args){if(!cmd.empty())cmd+=L' ';cmd+=quote(arg);}PROCESS_INFORMATION pi={};
    BOOL ok=CreateProcessW(args[0].c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);CloseHandle(wr);
    if(!ok){CloseHandle(rd);throw std::runtime_error("Cannot start compiler: "+utf8(args[0])+" (Windows "+std::to_string(GetLastError())+")");}
    char b[8192];DWORD n;while(ReadFile(rd,b,sizeof b,&n,nullptr)&&n){output.append(b,n);std::cout.write(b,n);std::cout.flush();}CloseHandle(rd);
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;
}
int wmain(int argc,wchar_t** argv){
    SetConsoleOutputCP(CP_UTF8);
    if(argc!=4){std::cerr<<"Usage: PracticeBuild build|clean|needed ROOT NAME\n";return 2;}
    try{
        std::string mode=utf8(argv[1]),name=utf8(argv[3]);validName(name);Workspace w;w.root=fs::absolute(argv[2]).lexically_normal();
        w.safe(w.root/L"setting.json");w.safe(w.practice(name));w.safe(w.logs(name));
        auto cpp=w.practice(name)/path(name+".cpp"),obj=w.practice(name)/path(name+".o"),exe=w.practice(name)/path(name+".exe");
        w.safe(cpp);w.safe(obj);w.safe(exe);
        if(mode=="needed")return 1;
        Lock lock;lock.acquire(w.root/L".apcs"/L"build.lock");
        if(mode=="clean"){fs::remove(obj);fs::remove(exe);return 0;}
        if(mode!="build")throw std::runtime_error("Unknown mode");
        auto settings=load(w.root/L"setting.json");validateSettings(settings);
        fs::path compiler=path(settings.at("compiler").get<std::string>());if(!fs::is_regular_file(compiler))throw std::runtime_error("Set a valid g++.exe path in APCS settings.");
        // Keep the exact source immutable until g++ has consumed it.
        HANDLE src=CreateFileW(cpp.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(src==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot lock source for a consistent build snapshot");
        struct Guard{HANDLE h;~Guard(){CloseHandle(h);}} guard{src};
        std::string snapshot=read(cpp),started=taipeiStamp(),token=id(),diagnostics,commands;int result=2;
        std::vector<std::wstring> compile={compiler.wstring(),L"-std="+wide(settings["cppStandard"].get<std::string>()),L"-g",L"-O2",L"-fdiagnostics-color=never",L"-finput-charset=UTF-8"};
        if(settings["warnings"].get<bool>()){compile.push_back(L"-Wall");compile.push_back(L"-Wextra");}
        compile.insert(compile.end(),{L"-c",cpp.wstring(),L"-o",obj.wstring()});
        auto execute=[&](const std::vector<std::wstring>& args){for(auto& a:args)commands+=utf8(quote(a))+" ";commands+="\n";return run(args,diagnostics);};
        // Write a pending log before starting the compiler, so crashes remain visible.
        auto log=w.logs(name)/path(token+".html");w.safe(log);
        auto html=[&](const std::string& status){return "<!doctype html><html lang=\"zh-Hant\"><head><meta charset=\"utf-8\"><title>APCS build log</title><style>body{font-family:system-ui;padding:24px}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#f5f5f5;padding:16px}</style></head><body><h1>Build log</h1><pre>"+esc("Problem: "+name+"\nStarted: "+started+"\nResult: "+status+"\nExit code: "+(status=="IN_PROGRESS"?"unknown":std::to_string(result))+"\nCommand: "+commands+"\n"+diagnostics)+"</pre><h2>Source snapshot at build start</h2><p>SHA-256: "+hash(snapshot)+"</p><h3>"+esc(name+".cpp")+"</h3><pre>"+esc(snapshot)+"</pre></body></html>\n";};
        write(log,html("IN_PROGRESS"),false);
        try{result=execute(compile);if(!result)result=execute({compiler.wstring(),obj.wstring(),L"-static",L"-o",exe.wstring()});}
        catch(const std::exception& e){diagnostics+="\n"+std::string(e.what());result=2;}
        write(log,html(result==0?"SUCCESS":"FAILED"),false);
        std::cout<<"\nAPCS log: "<<str(log)<<"\n";
        return result;
    }catch(const std::exception& e){std::cerr<<"APCS: "<<e.what()<<"\n";return 2;}
}
