#pragma once
#include "../third_party/json.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <iomanip>

namespace apcs {
namespace fs = std::filesystem;
using json = nlohmann::json;
inline std::string utf8(const std::wstring& s) {
    if(s.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
    std::string r(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr); return r;
}
inline std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
    if(!n) throw std::runtime_error("Invalid UTF-8");
    std::wstring r(n,0); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),r.data(),n); return r;
}
inline fs::path path(const std::string& s) { return fs::path(wide(s)); }
inline std::string str(const fs::path& p) { return utf8(p.wstring()); }
inline bool samePath(const fs::path& a,const fs::path& b){
    if(a.empty()||b.empty())return a.empty()&&b.empty();
    auto aa=fs::absolute(a).lexically_normal().wstring(),bb=fs::absolute(b).lexically_normal().wstring();
    return CompareStringOrdinal(aa.c_str(),-1,bb.c_str(),-1,TRUE)==CSTR_EQUAL;
}
inline std::string read(const fs::path& p) {
    std::ifstream f(p,std::ios::binary); if(!f) throw std::runtime_error("Cannot read: "+str(p));
    return std::string(std::istreambuf_iterator<char>(f),{});
}
// Detection is read-only. Ordinary C++ projects are never converted implicitly.
inline bool isPracticeProject(const fs::path& p){
    if(!samePath(p.filename(),L"APCSPractice.cbp")||!fs::is_regular_file(p))return false;
    auto xml=read(p);
    auto begin=xml.find("<Extensions>"),end=xml.find("</Extensions>",begin);
    if(begin==std::string::npos||end==std::string::npos)return false;
    auto marker=xml.find("<APCSPracticeStudio version=\"",begin);
    return marker!=std::string::npos&&marker<end;
}
inline std::string stamp(bool file=false) {
    SYSTEMTIME t; GetSystemTime(&t); char b[80];
    if(file) snprintf(b,sizeof b,"%04d%02d%02d-%02d%02d%02d-%03d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
    else snprintf(b,sizeof b,"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
    return b;
}
inline std::string id() { static unsigned seq=0; return stamp(true)+"-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++seq); }
inline std::string taipeiStamp(){
    FILETIME ft;GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER n;n.LowPart=ft.dwLowDateTime;n.HighPart=ft.dwHighDateTime;n.QuadPart+=8ULL*3600*10000000;
    ft.dwLowDateTime=n.LowPart;ft.dwHighDateTime=n.HighPart;SYSTEMTIME t;FileTimeToSystemTime(&ft,&t);char b[80];
    snprintf(b,sizeof b,"%04d-%02d-%02d %02d:%02d:%02d.%03d +08:00",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);return b;
}
inline void write(const fs::path& p,const std::string& s,bool backup=true) {
    fs::create_directories(p.parent_path()); auto tmp=p; tmp+=L".tmp";
    HANDLE h=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot write: "+str(p));
    DWORD n=0; bool ok=WriteFile(h,s.data(),(DWORD)s.size(),&n,nullptr)&&n==s.size()&&FlushFileBuffers(h); CloseHandle(h);
    if(!ok) throw std::runtime_error("Cannot flush: "+str(p));
    if(backup&&fs::exists(p)) { auto bak=p; bak+=L".bak"; if(!CopyFileW(p.c_str(),bak.c_str(),FALSE))throw std::runtime_error("Cannot back up: "+str(p)+" ("+std::to_string(GetLastError())+")"); }
    if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot replace: "+str(p));
}
inline void put(const fs::path& p,const json& j) { write(p,j.dump(2)+"\n"); }
inline json load(const fs::path& p) { return json::parse(read(p)); }
struct Lock {
    HANDLE h=INVALID_HANDLE_VALUE;
    void acquire(const fs::path& p) {
        release(); h=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_HIDDEN,nullptr);
        if(h==INVALID_HANDLE_VALUE) throw std::runtime_error("This workspace is already open, locked, or not writable.");
    }
    void release(){ if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);h=INVALID_HANDLE_VALUE;} }
    ~Lock(){release();}
};
inline bool same(const std::string& a,const std::string& b) { return CompareStringOrdinal(wide(a).c_str(),-1,wide(b).c_str(),-1,TRUE)==CSTR_EQUAL; }
inline void validName(const std::string& s) {
    auto w=wide(s);
    if(w.empty()||w.size()>100||w.front()==L' '||w.back()==L' ') throw std::runtime_error("Name must contain 1-100 characters and no leading/trailing spaces.");
    for(auto c:w) if(c<32||std::wstring(L".<>:\"/\\|?*%$!^&`").find(c)!=std::wstring::npos) throw std::runtime_error("Name contains a forbidden filename or command/macro character.");
    std::wstring u=w; std::transform(u.begin(),u.end(),u.begin(),::towupper);
    if(u==L"CON"||u==L"PRN"||u==L"AUX"||u==L"NUL"||u==L"CONIN$"||u==L"CONOUT$"||
       (u.size()==4&&(u.substr(0,3)==L"COM"||u.substr(0,3)==L"LPT")&&(u[3]>=L'1'&&u[3]<=L'9'))||
       (u.size()==4&&(u.substr(0,3)==L"COM"||u.substr(0,3)==L"LPT")&&(u[3]==L'\u00b9'||u[3]==L'\u00b2'||u[3]==L'\u00b3')))
        throw std::runtime_error("Windows reserved name.");
}
inline std::string esc(const std::string& s) {
    std::string r; for(char c:s) {switch(c){case '&':r+="&amp;";break;case '<':r+="&lt;";break;case '>':r+="&gt;";break;case '\"':r+="&quot;";break;case '\'':r+="&#39;";break;default:r+=c;}} return r;
}
inline std::wstring quote(const std::wstring& s) {
    std::wstring r=L"\""; unsigned sl=0; for(auto c:s){if(c==L'\\'){++sl;continue;} if(c==L'\"'){r.append(sl*2+1,L'\\');r+=c;}else{r.append(sl,L'\\');r+=c;} sl=0;}r.append(sl*2,L'\\');return r+L"\"";
}
inline json defaults() { return {
    {"version",1},{"autoStart",false},{"pauseOnSwitch",true},{"pauseOnBlur",false},{"idleMinutes",0},
    {"sort","newest"},{"showTime",true},{"rememberGroups",true},{"unfinishedExpanded",true},{"finishedExpanded",true},
    {"template","#include <iostream>\n\nint main()\n{\n    return 0;\n}\n"},
    {"enterNew",true},{"cppStandard","c++17"},{"warnings",true},{"logEveryBuild",true},
    {"reopenLast",true},{"showTrash",true},{"deleteToTrash",true},{"closeCppOnReturn",true},{"compiler",""}
}; }
inline void validateSettings(const json& s) {
    if(s.contains("deleteToTrash")&&!s["deleteToTrash"].is_boolean())throw std::runtime_error("Invalid setting: deleteToTrash");
    if(s.contains("closeCppOnReturn")&&!s["closeCppOnReturn"].is_boolean())throw std::runtime_error("Invalid setting: closeCppOnReturn");
    for(auto key:{"autoStart","pauseOnSwitch","pauseOnBlur","showTime","rememberGroups","unfinishedExpanded","finishedExpanded","enterNew","warnings","logEveryBuild","reopenLast","showTrash"})
        if(!s.contains(key)||!s[key].is_boolean()) throw std::runtime_error(std::string("Invalid setting: ")+key);
    if(!s.contains("idleMinutes")||!s["idleMinutes"].is_number_integer()||s["idleMinutes"].get<int>()<0||s["idleMinutes"].get<int>()>1440) throw std::runtime_error("Invalid idleMinutes");
    for(auto key:{"template","cppStandard","compiler","sort"}) if(!s.contains(key)||!s[key].is_string())throw std::runtime_error(std::string("Invalid setting: ")+key);
    if(s["cppStandard"]!="c++11"&&s["cppStandard"]!="c++14"&&s["cppStandard"]!="c++17"&&s["cppStandard"]!="c++20"&&s["cppStandard"]!="c++23")throw std::runtime_error("Invalid C++ standard");
    if(s["sort"]!="newest"&&s["sort"]!="oldest"&&s["sort"]!="name")throw std::runtime_error("Invalid sort");
}
inline json normalizeSettings(json s){
    validateSettings(s);
    if(!s.contains("deleteToTrash"))s["deleteToTrash"]=true;
    if(!s.contains("closeCppOnReturn"))s["closeCppOnReturn"]=true;
    s.erase("deletePermanentDefault");
    return s;
}
class Workspace {
    Lock lock;
public:
    fs::path root;
    json settings,state,times;
    fs::path cbp() const { return root/L"APCSPractice.cbp"; }
    fs::path practice(const std::string& n)const{validName(n);return root/L"allPractice"/path(n);}
    fs::path logs(const std::string& n)const{validName(n);return root/L"allLogs"/path(n);}
    void safe(const fs::path& p)const {
        auto a=fs::absolute(root).lexically_normal(), b=fs::absolute(p).lexically_normal();
        auto rel=b.lexically_relative(a);
        if(rel.empty()||rel==L"."||*rel.begin()==L"..") throw std::runtime_error("Unsafe path");
        auto walk=a; for(auto& part:rel){walk/=part; DWORD at=GetFileAttributesW(walk.c_str());if(at!=INVALID_FILE_ATTRIBUTES&&(at&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Linked paths are not supported: "+str(walk));}
    }
    static std::vector<std::string> missing(const fs::path& r){
        std::vector<std::string> v;for(auto n:{"APCSPractice.cbp","setting.json","allPracticeTime.json","allPracticeState.json","allPractice","allLogs"})if(!fs::exists(r/path(n)))v.push_back(n);return v;
    }
    void open(const fs::path& r,bool repair,const json& def,const fs::path& helper){
        root=fs::absolute(r).lexically_normal();if(!fs::is_directory(root))throw std::runtime_error("Not a folder");
        DWORD at=GetFileAttributesW(root.c_str()); if(at&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Linked workspace is not supported");
        lock.acquire(root/L".apcs-workspace.lock");
        try {
            for(auto n:{"allPractice","allLogs",".apcs",".trash","setting.json","allPracticeState.json","allPracticeTime.json","APCSPractice.cbp"})safe(root/path(n));
            if(!repair&&!missing(root).empty())throw std::runtime_error("Workspace files are missing");
            if(!fs::exists(root/L"setting.json"))put(root/L"setting.json",def);
            settings=normalizeSettings(load(root/L"setting.json"));
            fs::create_directories(root/L"allPractice"); fs::create_directories(root/L"allLogs");fs::create_directories(root/L".trash");
            if(!fs::exists(root/L"allPracticeState.json")){
                json rows=json::array();for(auto& e:fs::directory_iterator(root/L"allPractice"))if(e.is_directory()){
                    auto n=str(e.path().filename());validName(n);safe(e.path()); if(fs::exists(e.path()/path(n+".cpp")))rows.push_back({{"id",id()},{"name",n},{"state","unfinished"},{"createdAt",""},{"history",json::array()}});
                }put(root/L"allPracticeState.json",{{"version",1},{"practices",rows}});
            }
            state=load(root/L"allPracticeState.json");validateState();
            if(!fs::exists(root/L"allPracticeTime.json")){
                json v={{"version",1},{"practices",json::object()}};for(auto& row:state["practices"])v["practices"][row["id"].get<std::string>()]={{"elapsedMs",nullptr},{"started",false},{"timeKnown",false}};
                put(root/L"allPracticeTime.json",v);
            }
            times=load(root/L"allPracticeTime.json");validateTimes();
            fs::create_directories(root/L".apcs");safe(root/L".apcs"/L"PracticeBuild.exe");
            if(!fs::exists(helper))throw std::runtime_error("PracticeBuild.exe is missing from the plugin installation");
            if(!CopyFileW(helper.c_str(),(root/L".apcs"/L"PracticeBuild.exe").c_str(),FALSE))throw std::runtime_error("Cannot install workspace build helper");
            if(!fs::exists(cbp()))project();
        }catch(...){lock.release();throw;}
    }
    void close(){lock.release();root.clear();}
    void validateState()const{
        if(!state.is_object()||!state.contains("practices")||!state["practices"].is_array())throw std::runtime_error("Invalid allPracticeState.json");
        std::vector<std::string> names;std::set<std::string> ids;
        for(auto& row:state["practices"]){
            auto n=row.at("name").get<std::string>();validName(n);auto k=row.at("id").get<std::string>();validName(k);
            for(auto& old:names)if(same(old,n))throw std::runtime_error("Duplicate practice name");
            names.push_back(n);
            if(!ids.insert(k).second||!row.at("history").is_array()||(row.at("state")!="unfinished"&&row.at("state")!="finished"))throw std::runtime_error("Invalid practice state");
            safe(practice(n));safe(logs(n));
        }
    }
    void validateTimes()const{
        if(!times.is_object()||!times.contains("practices")||!times["practices"].is_object())throw std::runtime_error("Invalid allPracticeTime.json");
        for(auto it=times["practices"].begin();it!=times["practices"].end();++it){auto& t=it.value();if(!t.contains("elapsedMs")||(!t["elapsedMs"].is_null()&&(!t["elapsedMs"].is_number_integer()||t["elapsedMs"].get<int64_t>()<0)))throw std::runtime_error("Invalid elapsedMs");}
    }
    json& row(const std::string& k){for(auto& r:state["practices"])if(r["id"]==k)return r;throw std::runtime_error("Practice not found");}
    bool exists(const std::string& n)const{for(auto& r:state["practices"])if(same(r["name"].get<std::string>(),n))return true;return fs::exists(practice(n));}
    void save(){validateState();validateTimes();put(root/L"allPracticeTime.json",times);put(root/L"allPracticeState.json",state);}
    void saveSettings(){validateSettings(settings);put(root/L"setting.json",settings);}
    std::string add(const std::string& n){
        validName(n);if(exists(n)||fs::exists(logs(n)))throw std::runtime_error("Practice name already exists");
        safe(practice(n));safe(logs(n));fs::create_directories(practice(n));fs::create_directories(logs(n));
        write(practice(n)/path(n+".cpp"),settings["template"].get<std::string>());
        auto k=id();state["practices"].push_back({{"id",k},{"name",n},{"state","unfinished"},{"createdAt",stamp()},{"history",json::array()}});
        times["practices"][k]={{"elapsedMs",0},{"started",false},{"timeKnown",true},{"running",false},{"savedAt",stamp()}};
        save();project();return k;
    }
    void finish(const std::string& k,const std::string& result,const std::string& submission){
        auto& r=row(k);r["state"]="finished";r["result"]=result;r["submission"]=submission;
        r["history"].push_back({{"completedAt",stamp()},{"result",result},{"submission",submission},{"elapsedMs",times["practices"][k]["elapsedMs"]}});save();
    }
    void editResult(const std::string& k,const std::string& result,const std::string& submission){
        auto& r=row(k);r["result"]=result;r["submission"]=submission;
        if(!r["history"].empty()){auto& h=r["history"].back();h["result"]=result;h["submission"]=submission;h["editedAt"]=stamp();}save();
    }
    void reopen(const std::string& k){row(k)["state"]="unfinished";save();}
    void project(){
        std::string x="<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<CodeBlocks_project_file><FileVersion major=\"1\" minor=\"6\"/><Project>\n<Option title=\""+esc(str(root.filename()))+"\"/><Option makefile_is_custom=\"1\"/><Option compiler=\"gcc\"/>\n<Build>\n";
        for(auto& r:state["practices"]){auto n=r["name"].get<std::string>();std::string rel="allPractice/"+n+"/"+n;
            auto cmd=[&](const std::string& mode){return "&quot;$(PROJECT_DIR).apcs/PracticeBuild.exe&quot; "+mode+" &quot;$(PROJECT_DIR).&quot; &quot;"+esc(n)+"&quot;";};
            x+="<Target title=\""+esc(n)+"\"><Option output=\""+esc(rel)+".exe\" prefix_auto=\"0\" extension_auto=\"0\"/><Option object_output=\"allPractice/"+esc(n)+"/\"/><Option working_dir=\"allPractice/"+esc(n)+"/\"/><Option type=\"1\"/><Option compiler=\"gcc\"/><MakeCommands>";
            for(auto tag:{"Build","CompileFile","SilentBuild"})x+="<"+std::string(tag)+" command=\""+cmd("build")+"\"/>";
            x+="<Clean command=\""+cmd("clean")+"\"/><DistClean command=\""+cmd("clean")+"\"/><AskRebuildNeeded command=\""+cmd("needed")+"\"/></MakeCommands></Target>\n";
        }x+="</Build>\n";
        for(auto& r:state["practices"]){auto n=r["name"].get<std::string>();x+="<Unit filename=\"allPractice/"+esc(n)+"/"+esc(n)+".cpp\"><Option compilerVar=\"CPP\"/><Option target=\""+esc(n)+"\"/></Unit>\n";}
        x+="<Extensions><APCSPracticeStudio version=\"1\"/></Extensions></Project></CodeBlocks_project_file>\n";write(cbp(),x);
    }
    void trash(const std::string& k){
        auto r=row(k);auto n=r["name"].get<std::string>();auto d=root/L".trash"/path(k);safe(d);
        if(fs::exists(d))throw std::runtime_error("Trash entry exists");
        fs::create_directories(d);
        put(d/L"entry.json",{{"practice",r},{"time",times["practices"].value(k,json::object())},{"deletedAt",stamp()}});
        if(fs::exists(practice(n)))fs::rename(practice(n),d/L"practice");
        try{if(fs::exists(logs(n)))fs::rename(logs(n),d/L"logs");}catch(...){if(fs::exists(d/L"practice"))fs::rename(d/L"practice",practice(n));throw;}
        auto& a=state["practices"];a.erase(std::remove_if(a.begin(),a.end(),[&](auto& v){return v["id"]==k;}),a.end());times["practices"].erase(k);save();project();
    }
    std::vector<json> trashList()const{
        std::vector<json> v;for(auto& e:fs::directory_iterator(root/L".trash"))if(e.is_directory()){safe(e.path());if(fs::exists(e.path()/L"entry.json")){auto j=load(e.path()/L"entry.json");j["trashId"]=str(e.path().filename());v.push_back(j);}}return v;
    }
    void restore(const std::string& k,const std::string& n){
        validName(k);validName(n);if(exists(n)||fs::exists(logs(n)))throw std::runtime_error("Name already exists");auto d=root/L".trash"/path(k);safe(d);
        auto j=load(d/L"entry.json");auto r=j.at("practice");auto old=r.at("name").get<std::string>();validName(old);auto rid=r.at("id").get<std::string>();validName(rid);
        for(auto& v:state["practices"])if(v["id"]==rid)throw std::runtime_error("Practice ID already exists");
        safe(d/L"practice");safe(d/L"logs");safe(practice(n));safe(logs(n));
        if(fs::exists(d/L"practice")){fs::rename(d/L"practice",practice(n));if(n!=old)for(auto ext:{".cpp",".o",".exe"}){auto a=practice(n)/path(old+ext);auto b=practice(n)/path(n+ext);if(fs::exists(a))fs::rename(a,b);}}
        if(fs::exists(d/L"logs"))fs::rename(d/L"logs",logs(n));
        r["name"]=n;state["practices"].push_back(r);times["practices"][rid]=j.at("time");save();project();fs::remove(d/L"entry.json");fs::remove(d);
    }
    void purge(const std::string& k){validName(k);auto d=root/L".trash"/path(k);safe(d);for(auto& e:fs::recursive_directory_iterator(d))safe(e.path());fs::remove_all(d);}
};
class Timer {
    using Clock=std::chrono::steady_clock; Clock::time_point since;
public:
    std::string key;int64_t base=0;bool running=false,started=false,known=true;
    int64_t elapsed()const{return base+(running?std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-since).count():0);}
    void load(Workspace& w,const std::string& k){key=k;auto t=w.times["practices"].value(k,json::object());known=t.contains("elapsedMs")&&!t["elapsedMs"].is_null();base=known?t["elapsedMs"].get<int64_t>():0;started=t.value("started",false);running=false;}
    void start(){if(!known)throw std::runtime_error("Previous timer data is unknown. Restore its backup before continuing.");if(!running){since=Clock::now();running=true;started=true;}}
    void pause(int64_t deduct=0){
        // Idle time can predate this run. Never subtract it from earlier sessions.
        if(running)base+=std::max<int64_t>(0,elapsed()-base-std::max<int64_t>(0,deduct));
        running=false;
    }
    void save(Workspace& w){if(key.empty())return;w.times["practices"][key]={{"elapsedMs",known?json(elapsed()):json(nullptr)},{"started",started},{"running",running},{"timeKnown",known},{"savedAt",stamp()}};put(w.root/L"allPracticeTime.json",w.times);}
};
inline std::string duration(int64_t ms){auto s=ms/1000;std::ostringstream o;o<<std::setfill('0')<<std::setw(2)<<s/3600<<":"<<std::setw(2)<<(s/60)%60<<":"<<std::setw(2)<<s%60;return o.str();}
}
