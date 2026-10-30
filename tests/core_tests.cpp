#include "../src/core.hpp"
#include "../src/ui_lifecycle.hpp"
#include <iostream>
#include <thread>
using namespace apcs;
static int checks=0;
static void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool rejected=false;try{f();}catch(...){rejected=true;}check(rejected,message);}
int wmain(){
    try{
        UiLifecycle lifecycle;
        check(lifecycle.beginRender(),"Initial panel render blocked");auto oldList=lifecycle.epoch();
        check(!lifecycle.accepts(oldList),"Tree notifications accepted while rebuilding");lifecycle.endRender();
        check(lifecycle.accepts(oldList),"Valid list event rejected");
        check(lifecycle.beginAction(),"Enter-practice action blocked");
        // Simulate nested SDK callbacks while Open(CPP) still owns page state.
        check(!lifecycle.beginRender(),"Nested event loop destroyed the active page");
        check(!lifecycle.beginAction(),"Nested navigation was allowed");
        check(!lifecycle.accepts(oldList),"Selection notification reentered during CPP open");
        lifecycle.endAction();check(lifecycle.beginRender(),"Deferred practice page could not render");
        check(!lifecycle.accepts(oldList),"Deleted list event was accepted");lifecycle.endRender();
        check(!lifecycle.accepts(oldList),"Queued event from the old page survived navigation");
        check(lifecycle.accepts(lifecycle.epoch()),"New page controls do not work");
        auto root=fs::current_path()/L"build"/path("中文 空白測試-"+id());fs::create_directories(root);
        auto helper=fs::current_path()/L"build"/L"PracticeBuild.exe";auto def=defaults();
        check(def["deleteToTrash"]==true,"Delete-to-trash must be enabled by default");
        auto legacy=def;legacy.erase("deleteToTrash");legacy["deletePermanentDefault"]=true;
        check(normalizeSettings(legacy)["deleteToTrash"]==true,"Legacy settings must default to trash");
        check(!normalizeSettings(legacy).contains("deletePermanentDefault"),"Obsolete setting was retained");
        auto permanent=def;permanent["deleteToTrash"]=false;
        check(normalizeSettings(permanent)["deleteToTrash"]==false,"Explicit permanent-delete preference lost");
        auto bad=def;bad["deleteToTrash"]="true";rejects([&]{normalizeSettings(bad);},"Invalid trash preference accepted");
        check(def["closeCppOnReturn"]==true,"Close-CPP-on-return must default to enabled");
        auto oldSettings=def;oldSettings.erase("closeCppOnReturn");
        check(normalizeSettings(oldSettings)["closeCppOnReturn"]==true,"Existing settings did not receive close-tab default");
        auto keepTab=def;keepTab["closeCppOnReturn"]=false;
        check(normalizeSettings(keepTab)["closeCppOnReturn"]==false,"Keep-tab preference was lost");
        auto invalidClose=def;invalidClose["closeCppOnReturn"]=1;
        rejects([&]{normalizeSettings(invalidClose);},"Invalid close-tab setting accepted");
        Workspace w;w.open(root,true,def,helper);
        for(auto s:{"","a.b","A/B","a\\b","a:","CON","nul","COM1","LPT9","x ","a?b",".."})rejects([&]{w.add(s);},"Invalid name accepted");
        auto k=w.add("c291－小群體");auto k2=w.add("Example");
        rejects([&]{w.add("example");},"Case insensitive duplicate accepted");
        check(fs::exists(w.practice("c291－小群體")/path("c291－小群體.cpp")),"Unicode CPP missing");
        auto projectText=read(w.cbp());check(projectText.find("allPractice/c291－小群體/c291－小群體.cpp")!=std::string::npos,"Project source path missing");
        check(isPracticeProject(w.cbp()),"Generated practice project was not detected");
        check(!isPracticeProject(root/L"missing.cbp"),"Missing project detected");
        auto ordinary=root/L"ordinary"/L"APCSPractice.cbp";write(ordinary,"<CodeBlocks_project_file><Project><Extensions/></Project></CodeBlocks_project_file>");
        check(!isPracticeProject(ordinary),"Ordinary project was implicitly converted");
        write(ordinary,"<CodeBlocks_project_file><Project><Extensions>\n  <APCSPracticeStudio version=\"1\" />\n</Extensions></Project></CodeBlocks_project_file>");
        check(isPracticeProject(ordinary),"IDE-formatted practice project was not detected");
        check(samePath(root/L"ALLPRACTICE"/L".."/L"APCSPractice.cbp",w.cbp()),"Normalized case-insensitive path match failed");
        check(!samePath(root/L"other.cbp",w.cbp()),"Different active projects treated as identical");
        check(projectText.find("$(PROJECT_DIR).apcs/PracticeBuild.exe")!=std::string::npos,"Build helper missing");
        Workspace other;rejects([&]{other.open(root,true,def,helper);},"Workspace lock allowed two writers");
        Timer t;t.load(w,k);t.start();std::this_thread::sleep_for(std::chrono::milliseconds(45));t.pause();auto elapsed=t.elapsed();check(elapsed>=30,"Timer failed");
        std::this_thread::sleep_for(std::chrono::milliseconds(40));check(t.elapsed()==elapsed,"Paused time increased");t.save(w);
        Timer idleTimer;idleTimer.base=27735;idleTimer.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));idleTimer.pause(60000);
        check(idleTimer.elapsed()==27735,"Idle deduction reduced an earlier session");
        idleTimer.pause(60000);check(idleTimer.elapsed()==27735,"Repeated pause reduced saved time");
        idleTimer.start();std::this_thread::sleep_for(std::chrono::milliseconds(30));idleTimer.pause(5);
        check(idleTimer.elapsed()>=27755,"Idle deduction lost the active session");
        auto idleElapsed=idleTimer.elapsed();idleTimer.start();idleTimer.pause(-1000);
        check(idleTimer.elapsed()>=idleElapsed&&idleTimer.elapsed()<idleElapsed+500,"Negative deduction added fictitious time");
        w.finish(k,"AC","TEST-001");check(w.row(k)["state"]=="finished","Finish state missing");w.reopen(k);check(w.row(k)["history"].size()==1,"Reopen lost history");
        w.finish(k,"NA","");check(w.row(k)["history"].size()==2,"Finish overwrote history");w.editResult(k,"AC","99");check(w.row(k)["history"].size()==2,"Edit duplicated history");
        auto log=w.logs("c291－小群體")/L"example.html";write(log,"old-log");w.trash(k);
        check(w.trashList().size()==1,"Trash missing");check(w.state["practices"].size()==1,"Trashed row remains");
        auto replacement=w.add("c291－小群體");(void)replacement;rejects([&]{w.restore(k,"c291－小群體");},"Restore overwrote existing practice");
        w.restore(k,"復原題目");check(fs::exists(w.practice("復原題目")/path("復原題目.cpp")),"Restored CPP not renamed");
        check(read(w.logs("復原題目")/L"example.html")=="old-log","Restore lost log");check(w.times["practices"][k]["elapsedMs"].get<int64_t>()==elapsed,"Restore lost timer");
        check(w.row(k)["history"].size()==2,"Restore lost history");w.trash(k2);w.purge(k2);check(w.trashList().empty(),"Purge failed");
        rejects([&]{w.safe(root.parent_path()/L"escape");},"Path traversal accepted");
        check(fs::exists(root/L"allPracticeTime.json.bak"),"Backup missing");
        w.close();Workspace loaded;loaded.open(root,false,def,helper);check(loaded.row(k)["state"]=="finished","State did not persist");
        Timer t2;t2.load(loaded,k);check(!t2.running&&t2.elapsed()==elapsed,"Reload timer wrong");loaded.close();
        // Missing timers must never silently invent a zero duration for an existing practice.
        fs::remove(root/L"allPracticeTime.json");Workspace repaired;repaired.open(root,true,def,helper);Timer unknown;unknown.load(repaired,k);check(!unknown.known,"Repaired duration invented");rejects([&]{unknown.start();},"Unknown timer started");repaired.close();
        write(fs::current_path()/L"build"/L"last-test-root.txt",str(root),false);
        std::cout<<"PASS "<<checks<<" checks. Fixtures: "<<str(root)<<"\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
