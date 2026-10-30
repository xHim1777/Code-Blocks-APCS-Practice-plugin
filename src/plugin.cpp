#include <sdk.h>
#include <cbplugin.h>
#include <manager.h>
#include <configmanager.h>
#include <projectmanager.h>
#include <cbproject.h>
#include <cbworkspace.h>
#include <editormanager.h>
#include <compilerfactory.h>
#include <compiler.h>
#include <sdk_events.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/scrolwin.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/textdlg.h>
#include <wx/choicdlg.h>
#include <wx/msgdlg.h>
#include <wx/menu.h>
#include <wx/timer.h>
#include <wx/filename.h>
#include <functional>
#include <map>
#include "core.hpp"
#include "ui_lifecycle.hpp"
using namespace apcs;
static wxString W(const std::string& s){return wxString::FromUTF8(s.c_str());}
static std::string U(const wxString& s){return std::string(s.utf8_str());}
static fs::path P(const wxString& s){return fs::path(s.ToStdWstring());}
static void resourceAnchor(){}
class PracticeData:public wxTreeItemData{public:std::string key;explicit PracticeData(std::string k):key(std::move(k)){};};

class APCSPracticeStudio:public cbPlugin {
    wxPanel* panel=nullptr;wxBoxSizer* body=nullptr;wxStaticText* clockLabel=nullptr;wxButton* timerButton=nullptr;wxButton* finishButton=nullptr;
    wxTreeCtrl* tree=nullptr;wxTreeItemId undone,done;
    wxTimer ticker;std::unique_ptr<Workspace> ws;std::map<std::string,Timer> timers;std::string current;bool inTrash=false,refreshing=false,shutting=false;
    int ticks=0;int menuId=wxWindow::NewControlId();json globalDefaults;
    bool syncQueued=false,syncing=false,syncRetry=false;fs::path declinedProject;
    UiLifecycle ui;bool renderQueued=false,renderPending=false,actionQueued=false;
    cbProject* project()const{return ws?Manager::Get()->GetProjectManager()->IsOpen(wxString(ws->cbp().wstring())):nullptr;}
    bool busy()const{return Manager::Get()->GetProjectManager()->GetIsRunning()!=nullptr;}
    void guard(std::function<void()> f){try{f();}catch(const std::exception& e){wxMessageBox(W(e.what()),W("APCS 練習：操作未完成"),wxOK|wxICON_ERROR,panel);}}
    void deferRender(){
        renderPending=true;if(renderQueued||shutting)return;renderQueued=true;
        CallAfter([this]{renderQueued=false;if(!panel||shutting)return;
            if(ui.busy()||syncing||refreshing)return;
            renderPending=false;guard([this]{render();});
        });
    }
    void runAction(std::function<void()> fn){
        if(shutting||!ui.beginAction())return;
        struct Finish {APCSPracticeStudio* self;~Finish(){self->ui.endAction();if(self->renderPending)self->deferRender();if(self->syncRetry)self->queueProjectSync();}} finish{this};
        guard(std::move(fn));
    }
    void postAction(std::function<void()> fn,std::uint64_t epoch){
        if(actionQueued||shutting||!ui.accepts(epoch))return;
        actionQueued=true;
        CallAfter([this,fn,epoch]{actionQueued=false;if(ui.accepts(epoch))runAction(fn);});
    }
    void trace(const char* stage){
        if(!ws)return;
        try{auto p=ws->root/L".apcs"/L"ui-trace.log";ws->safe(p);
            HANDLE h=CreateFileW(p.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(h!=INVALID_HANDLE_VALUE){auto line=stamp()+" "+stage+"\r\n";DWORD n;WriteFile(h,line.data(),(DWORD)line.size(),&n,nullptr);FlushFileBuffers(h);CloseHandle(h);}
        }catch(...){} // Diagnostic output must never prevent practicing.
    }
    void label(wxWindow* parent,wxSizer* s,const wxString& value){s->Add(new wxStaticText(parent,wxID_ANY,value),0,wxALL|wxEXPAND,7);}
    wxButton* button(wxWindow* parent,wxSizer* s,const wxString& text,std::function<void()> fn){
        auto b=new wxButton(parent,wxID_ANY,text);s->Add(b,0,wxALL|wxEXPAND,4);
        if(parent!=panel){b->Bind(wxEVT_BUTTON,[this,fn](wxCommandEvent&){guard(fn);});}
        else{auto epoch=ui.epoch();b->Bind(wxEVT_BUTTON,[this,fn,epoch](wxCommandEvent&){postAction(fn,epoch);});}
        return b;
    }
    ConfigManager* cfg(){return Manager::Get()->GetConfigManager("apcs_practice_studio");}
    void saveGlobals(){cfg()->Write("defaults",W(globalDefaults.dump()));}
    fs::path helper(){
        HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&resourceAnchor,&module);
        HRSRC r=FindResourceW(module,MAKEINTRESOURCEW(101),RT_RCDATA);if(!r)throw std::runtime_error("Embedded build helper is missing");
        HGLOBAL h=LoadResource(module,r);auto data=(const char*)LockResource(h);auto size=SizeofResource(module,r);
        if(!data||!size)throw std::runtime_error("Cannot load embedded build helper");
        auto dest=P(ConfigManager::GetTempFolder())/L"APCSPracticeStudio-1.0.0"/L"PracticeBuild.exe";
        std::string bytes(data,size);if(!fs::exists(dest)||read(dest)!=bytes)write(dest,bytes,false);return dest;
    }
    std::string detectedCompiler(){
        auto c=CompilerFactory::GetCompiler("gcc");if(c){auto p=P(c->GetMasterPath())/L"bin"/P(c->GetPrograms().CPP);if(fs::exists(p))return str(p);}
        auto p=P(ConfigManager::GetExecutableFolder())/L"MinGW"/L"bin"/L"g++.exe";return fs::exists(p)?str(p):"";
    }
    void remember(){if(ws){cfg()->Write("last_folder",wxString(ws->root.wstring()));cfg()->Write("reopen_last",ws->settings["reopenLast"].get<bool>());}}
    void pauseAll(){if(!ws)return;for(auto& v:timers){v.second.pause();v.second.save(*ws);}}
    void releaseWorkspace(){
        if(!ws)return;
        pauseAll();remember();ws->close();ws.reset();timers.clear();current.clear();inTrash=false;
        deferRender();
    }
    void syncActiveProject(){
        if(shutting||refreshing||syncing||!panel)return;
        if(ui.busy()){syncRetry=true;return;}
        auto pm=Manager::Get()->GetProjectManager();
        if(busy()||pm->IsLoadingOrClosing()){syncRetry=true;return;}
        syncRetry=false;
        auto active=pm->GetActiveProject();
        auto filename=active?P(active->GetFilename()):fs::path();
        if(!samePath(filename,declinedProject))declinedProject.clear();
        if(ws&&samePath(filename,ws->cbp()))return;
        // Switching the IDE's active project detaches the Panel only; it never closes projects.
        releaseWorkspace();
        if(filename.empty()||samePath(filename,declinedProject)||!isPracticeProject(filename))return;
        syncing=true;
        try{
            openFolder(filename.parent_path(),false,true);
            if(!ws)declinedProject=filename; // Do not repeat a cancelled repair prompt.
            syncing=false;if(renderPending)deferRender();
        }catch(...){declinedProject=filename;syncing=false;if(renderPending)deferRender();throw;}
    }
    void queueProjectSync(){
        if(syncQueued||shutting||refreshing||syncing)return;
        syncQueued=true;
        CallAfter([this]{syncQueued=false;if(!shutting&&panel)guard([this]{syncActiveProject();});});
    }
    Timer& timer(const std::string& k){auto it=timers.find(k);if(it==timers.end()){Timer t;t.load(*ws,k);it=timers.emplace(k,t).first;}return it->second;}
    void saveTimers(){if(ws)for(auto& v:timers)v.second.save(*ws);}
    void requireIdle(){if(busy())throw std::runtime_error("Wait until the current build/run has stopped.");}
    void prepareHiddenProjectTree(){
        auto t=Manager::Get()->GetProjectManager()->GetUI().GetTree();
        // CB 25.03 RebuildTree walks GetNextVisible from a saved scroll item,
        // even when Management is hidden. Reset to the root before that walk;
        // RebuildTree itself expands the root again. No IDE settings are changed.
        if(t&&!t->IsShownOnScreen()){
            auto root=t->GetRootItem();if(root.IsOk())t->Collapse(root);
        }
    }
    void selectBuildTarget(cbProject* p,const std::string& name){
        if(!p->SetActiveBuildTarget(W(name)))throw std::runtime_error("Build target is missing. Reopen or repair the practice project.");
        // CompilerGCC prefers the workspace target over the project target.
        if(auto workspace=Manager::Get()->GetProjectManager()->GetWorkspace())workspace->SetPreferredTarget(W(name));
        // CompilerGCC listens to TARGETS_MODIFIED, not BUILDTARGET_SELECTED.
        // Synchronize its cached target and toolbar before another build can run.
        CodeBlocksEvent e(cbEVT_PROJECT_TARGETS_MODIFIED);e.SetProject(p);
        Manager::Get()->ProcessEvent(e);
    }
    bool closeProjectForChange(){
        requireIdle();if(!ws)return true;auto p=project();if(!p)return true;
        prepareHiddenProjectTree();
        if(!p->SaveAllFiles())return false;
        refreshing=true;bool ok=Manager::Get()->GetProjectManager()->CloseProject(p,false);refreshing=false;return ok;
    }
    void loadProject(){
        auto p=Manager::Get()->GetProjectManager()->LoadProject(wxString(ws->cbp().wstring()),true);
        if(!p)throw std::runtime_error("Cannot open the practice .cbp project");
        if(!current.empty()){auto n=ws->row(current)["name"].get<std::string>();selectBuildTarget(p,n);}
    }
    bool closeWorkspace(){
        if(!ws)return true;
        if(!closeProjectForChange())return false;
        releaseWorkspace();return true;
    }
    void restoreBackupPrompt(const fs::path& root){
        for(auto name:{"setting.json","allPracticeState.json","allPracticeTime.json"}){
            auto p=root/path(name);if(!fs::exists(p))continue;
            bool valid=true;try{auto j=load(p);if(std::string(name)=="setting.json")validateSettings(j);else{Workspace tmp;tmp.root=root;if(std::string(name)=="allPracticeState.json"){tmp.state=j;tmp.validateState();}else{tmp.times=j;tmp.validateTimes();}}}catch(...){valid=false;}
            if(valid)continue;
            auto bak=p;bak+=L".bak";
            if(!fs::exists(bak))throw std::runtime_error(std::string(name)+" is damaged; no backup exists. Original data was kept.");
            auto msg=W(std::string(name))+W(" 無法讀取。是否從 .bak 備份還原？\n損壞檔案會另外保留。");
            if(wxMessageBox(msg,W("還原備份"),wxYES_NO|wxICON_WARNING,panel)!=wxYES)throw std::runtime_error("Backup restoration cancelled");
            auto j=load(bak);if(std::string(name)=="setting.json")validateSettings(j);else{Workspace tmp;tmp.root=root;if(std::string(name)=="allPracticeState.json"){tmp.state=j;tmp.validateState();}else{tmp.times=j;tmp.validateTimes();}}
            auto damaged=p;damaged+=path(".damaged-"+id()).wstring();fs::copy_file(p,damaged);write(p,read(bak),false);
        }
    }
    void openFolder(const fs::path& root,bool fresh=false,bool fromIDE=false){
        requireIdle();
        if(ws&&samePath(root,ws->root)){if(!fromIDE)loadProject();return;}
        if(ws){if(fromIDE)releaseWorkspace();else if(!closeWorkspace())return;}
        // Probe the lock before offering any repair operation.
        {Lock check;check.acquire(root/L".apcs-workspace.lock");restoreBackupPrompt(root);}
        auto missing=Workspace::missing(root);if(!missing.empty()&&!fresh){
            std::string msg="缺少下列項目：\n";for(auto& x:missing)msg+="• "+x+"\n";
            msg+="\n是否建立缺少的檔案？現有檔案不會被覆寫。\n若計時檔遺失，既有題目的時間會標示為未知。";
            if(wxMessageBox(W(msg),W("修復練習資料夾"),wxYES_NO|wxICON_QUESTION,panel)!=wxYES)return;
        }
        auto next=std::make_unique<Workspace>();auto d=globalDefaults;
        if(d["compiler"].get<std::string>().empty())d["compiler"]=detectedCompiler();
        next->open(root,true,d,helper());ws=std::move(next);current.clear();timers.clear();inTrash=false;
        try{if(!fromIDE)loadProject();remember();declinedProject.clear();}catch(...){ws.reset();throw;}deferRender();
    }
    void chooseOpen(){wxDirDialog d(panel,W("選擇練習資料夾"));if(d.ShowModal()==wxID_OK)openFolder(P(d.GetPath()));}
    void createFolder(){
        wxDirDialog d(panel,W("選擇儲存位置"));if(d.ShowModal()!=wxID_OK)return;
        wxTextEntryDialog name(panel,W("輸入練習資料夾名稱"),W("新建練習資料夾"));if(name.ShowModal()!=wxID_OK)return;
        auto n=U(name.GetValue());validName(n);auto root=P(d.GetPath())/path(n);if(fs::exists(root))throw std::runtime_error("Folder already exists");
        requireIdle();if(ws&&!closeWorkspace())return;fs::create_directories(root);openFolder(root,true);
    }
    void enter(const std::string& k){
        trace("enter: begin");
        requireIdle();auto& r=ws->row(k);auto n=r["name"].get<std::string>();auto cpp=ws->practice(n)/path(n+".cpp");
        ws->safe(cpp);if(!fs::exists(cpp))throw std::runtime_error("CPP file is missing: "+str(cpp));
        if(!current.empty()&&current!=k&&ws->settings["pauseOnSwitch"].get<bool>()){timer(current).pause();timer(current).save(*ws);}
        auto p=project();if(!p){loadProject();p=project();}
        trace("enter: activate project");
        if(Manager::Get()->GetProjectManager()->GetActiveProject()!=p)Manager::Get()->GetProjectManager()->SetProject(p);
        trace("enter: select target");
        selectBuildTarget(p,n);
        trace("enter: open CPP");
        if(!Manager::Get()->GetEditorManager()->Open(wxString(cpp.wstring())))throw std::runtime_error("Cannot open the practice CPP");
        trace("enter: CPP opened");current=k;inTrash=false;
        auto& t=timer(k);if(r["state"]=="unfinished"&&ws->settings["autoStart"].get<bool>()){t.start();t.save(*ws);}deferRender();trace("enter: done");
    }
    void leavePractice(){
        if(!ws||current.empty())return;
        auto k=current;
        if(ws->settings.value("closeCppOnReturn",true)){
            auto n=ws->row(k)["name"].get<std::string>();
            auto cpp=ws->practice(n)/path(n+".cpp");ws->safe(cpp);
            auto em=Manager::Get()->GetEditorManager();
            auto filename=wxString(cpp.wstring());
            // Close() asks about unsaved changes and returns false on Cancel.
            if(em->IsOpen(filename)&&!em->Close(filename))return;
        }
        timer(k).pause();timer(k).save(*ws);current.clear();deferRender();
    }
    void addPractice(){
        requireIdle();wxTextEntryDialog d(panel,W("練習名稱（不可重名或包含 . 等特殊字元）"),W("新增練習"));if(d.ShowModal()!=wxID_OK)return;
        auto n=U(d.GetValue());validName(n);if(ws->exists(n)||fs::exists(ws->logs(n)))throw std::runtime_error("Practice name already exists");
        trace("add: close project");if(!closeProjectForChange())return;
        pauseAll();
        try{trace("add: create files");auto k=ws->add(n);trace("add: reload project");loadProject();trace("add: project loaded");if(ws->settings["enterNew"].get<bool>())enter(k);else deferRender();}
        catch(...){loadProject();throw;}
    }
    void complete(bool edit=false){
        auto& r=ws->row(current);wxDialog d(panel,wxID_ANY,W(edit?"修改作答資料":"完成練習"),wxDefaultPosition,wxSize(390,300));auto s=new wxBoxSizer(wxVERTICAL);
        label(&d,s,W("練習："+r["name"].get<std::string>()));label(&d,s,W("作答結果（自由輸入，可留空，例如 AC、NA）"));
        auto result=new wxTextCtrl(&d,wxID_ANY,W(r.value("result",std::string())));s->Add(result,0,wxALL|wxEXPAND,7);
        label(&d,s,W("解題編號（可留空）"));auto submission=new wxTextCtrl(&d,wxID_ANY,W(r.value("submission",std::string())));s->Add(submission,0,wxALL|wxEXPAND,7);
        auto ok=new wxButton(&d,wxID_OK,W(edit?"儲存修改":"確認完成"));auto buttons=new wxBoxSizer(wxHORIZONTAL);buttons->Add(ok,1,wxALL,5);buttons->Add(new wxButton(&d,wxID_CANCEL,W("取消")),1,wxALL,5);s->Add(buttons,0,wxEXPAND);d.SetSizerAndFit(s);
        if(d.ShowModal()!=wxID_OK)return;
        if(edit)ws->editResult(current,U(result->GetValue()),U(submission->GetValue()));else{timer(current).pause();timer(current).save(*ws);ws->finish(current,U(result->GetValue()),U(submission->GetValue()));}
        deferRender();
    }
    std::string selectPractice(){
        wxArrayString labels;std::vector<std::string> keys;for(auto& r:ws->state["practices"]){labels.Add(W(r["name"].get<std::string>()));keys.push_back(r["id"].get<std::string>());}
        if(keys.empty())return {};
        wxSingleChoiceDialog d(panel,W("選擇要刪除的練習"),W("刪除練習"),labels);if(d.ShowModal()!=wxID_OK)return {};return keys.at(d.GetSelection());
    }
    void deletePractice(){
        requireIdle();auto k=selectPractice();if(k.empty())return;auto n=ws->row(k)["name"].get<std::string>();
        bool toTrash=ws->settings.value("deleteToTrash",true);
        auto message=toTrash?"將「"+n+"」移至垃圾桶？\n包含 CPP、編譯產物、log、時間及狀態，之後可以從垃圾桶復原。":"永久刪除「"+n+"」？\n包含 CPP、編譯產物、log、時間及狀態，無法復原。";
        wxMessageDialog d(panel,W(message),W("刪除練習"),wxOK|wxCANCEL|(toTrash?wxICON_QUESTION:wxICON_WARNING));
        d.SetOKCancelLabels(W(toTrash?"移至垃圾桶":"永久刪除"),W("取消"));if(d.ShowModal()!=wxID_OK)return;
        if(!toTrash&&wxMessageBox(W("確定永久刪除此練習？此操作無法復原。"),W("確認永久刪除"),wxYES_NO|wxNO_DEFAULT|wxICON_WARNING,panel)!=wxYES)return;
        auto cpp=ws->practice(n)/path(n+".cpp");auto em=Manager::Get()->GetEditorManager();if(em->IsOpen(wxString(cpp.wstring()))&&!em->Close(wxString(cpp.wstring())))return;
        if(!closeProjectForChange())return;
        timer(k).pause();timer(k).save(*ws);timers.erase(k);if(current==k)current.clear();
        try{ws->trash(k);if(!toTrash)ws->purge(k);loadProject();deferRender();}catch(...){loadProject();throw;}
    }
    void settingsDialog(bool global){
        json s=global?globalDefaults:ws->settings;
        wxDialog d(panel,wxID_ANY,W(global?"設定預設設定":"練習資料夾設定"),wxDefaultPosition,wxSize(560,700),wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
        auto outer=new wxBoxSizer(wxVERTICAL);auto scroll=new wxScrolledWindow(&d);scroll->SetScrollRate(0,15);auto layout=new wxBoxSizer(wxVERTICAL);
        std::map<std::string,wxCheckBox*> checks;
        const std::pair<const char*,const char*> opts[]={
            {"autoStart","進入練習時自動開始／繼續計時"},{"pauseOnSwitch","切換練習時暫停上一題"},
            {"pauseOnBlur","Code::Blocks 失去焦點時自動暫停"},{"showTime","列表顯示累計時間"},
            {"rememberGroups","記住分類展開狀態"},{"enterNew","新增後自動進入練習"},
            {"warnings","啟用 -Wall -Wextra"},{"logEveryBuild","每次編譯均儲存 log"},
            {"reopenLast","啟動時開啟上次的練習資料夾"},{"showTrash","顯示垃圾桶按鈕"},
            {"deleteToTrash","刪除時移至垃圾桶"},
            {"closeCppOnReturn","返回練習列表時關閉該題 CPP 分頁"}};
        for(auto& opt:opts){auto c=new wxCheckBox(scroll,wxID_ANY,W(opt.second));c->SetValue(s[opt.first].get<bool>());layout->Add(c,0,wxALL,6);checks[opt.first]=c;}
        checks["logEveryBuild"]->Enable(false); // Every build is a required invariant.
        label(scroll,layout,W("閒置自動暫停：0 = 關閉；1–1440 分鐘\n依整台電腦的鍵盤／滑鼠活動判斷，扣除閒置時段。"));
        auto idle=new wxSpinCtrl(scroll,wxID_ANY);idle->SetRange(0,1440);idle->SetValue(s["idleMinutes"].get<int>());layout->Add(idle,0,wxALL|wxEXPAND,6);
        label(scroll,layout,W("列表排序"));wxArrayString sorts;sorts.Add(W("建立時間：新到舊"));sorts.Add(W("建立時間：舊到新"));sorts.Add(W("名稱"));auto sort=new wxChoice(scroll,wxID_ANY,wxDefaultPosition,wxDefaultSize,sorts);sort->SetSelection(s["sort"]=="newest"?0:s["sort"]=="oldest"?1:2);layout->Add(sort,0,wxALL|wxEXPAND,6);
        label(scroll,layout,W("C++ 標準"));wxArrayString standards;for(auto v:{"c++11","c++14","c++17","c++20","c++23"})standards.Add(W(v));auto standard=new wxChoice(scroll,wxID_ANY,wxDefaultPosition,wxDefaultSize,standards);standard->SetStringSelection(W(s["cppStandard"].get<std::string>()));layout->Add(standard,0,wxALL|wxEXPAND,6);
        label(scroll,layout,W("g++.exe 完整路徑（留空時自動偵測）"));auto compiler=new wxTextCtrl(scroll,wxID_ANY,W(s["compiler"].get<std::string>()));layout->Add(compiler,0,wxALL|wxEXPAND,6);
        button(scroll,layout,W("選擇編譯器"),[&]{wxFileDialog f(&d,W("選擇 g++.exe"),wxEmptyString,wxEmptyString,W("Executable (*.exe)|*.exe"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);if(f.ShowModal()==wxID_OK)compiler->SetValue(f.GetPath());});
        label(scroll,layout,W("新練習的 C++ 模板"));auto code=new wxTextCtrl(scroll,wxID_ANY,W(s["template"].get<std::string>()),wxDefaultPosition,wxSize(-1,180),wxTE_MULTILINE);layout->Add(code,0,wxALL|wxEXPAND,6);
        scroll->SetSizer(layout);outer->Add(scroll,1,wxEXPAND|wxALL,6);outer->Add(d.CreateButtonSizer(wxOK|wxCANCEL),0,wxALL|wxALIGN_RIGHT,8);d.SetSizer(outer);
        if(d.ShowModal()!=wxID_OK)return;
        for(auto& c:checks)s[c.first]=c.second->GetValue();
        s["idleMinutes"]=idle->GetValue();s["sort"]=sort->GetSelection()==0?"newest":sort->GetSelection()==1?"oldest":"name";
        s["cppStandard"]=U(standard->GetStringSelection());s["template"]=U(code->GetValue());s["compiler"]=U(compiler->GetValue());
        if(s["compiler"].get<std::string>().empty())s["compiler"]=detectedCompiler();
        validateSettings(s);
        if(global){globalDefaults=s;saveGlobals();cfg()->Write("reopen_last",s["reopenLast"].get<bool>());}else{requireIdle();ws->settings=s;ws->saveSettings();remember();}deferRender();
    }
    void showHistory(){
        auto& r=ws->row(current);std::string text;for(auto& h:r["history"]){text+=h.value("completedAt",std::string())+"\n結果："+h.value("result",std::string())+"　解題編號："+h.value("submission",std::string())+"\n累計時間："+(h.contains("elapsedMs")&&!h["elapsedMs"].is_null()?duration(h["elapsedMs"].get<int64_t>()):"未知")+"\n\n";}
        wxDialog d(panel,wxID_ANY,W("完成紀錄"),wxDefaultPosition,wxSize(500,400),wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);auto s=new wxBoxSizer(wxVERTICAL);s->Add(new wxTextCtrl(&d,wxID_ANY,W(text),wxDefaultPosition,wxDefaultSize,wxTE_MULTILINE|wxTE_READONLY),1,wxALL|wxEXPAND,8);s->Add(d.CreateButtonSizer(wxOK),0,wxALL|wxALIGN_RIGHT,6);d.SetSizer(s);d.ShowModal();
    }
    void renderTrash(){
        button(panel,body,W("＜ 返回"),[this]{inTrash=false;deferRender();});label(panel,body,W("垃圾桶"));
        auto entries=ws->trashList();wxArrayString labels;for(auto& j:entries)labels.Add(W(j["practice"]["name"].get<std::string>()));
        auto list=new wxChoice(panel,wxID_ANY,wxDefaultPosition,wxDefaultSize,labels);if(!entries.empty())list->SetSelection(0);body->Add(list,0,wxALL|wxEXPAND,7);
        button(panel,body,W("復原選取練習"),[this,list,entries]{
            requireIdle();int i=list->GetSelection();if(i==wxNOT_FOUND)return;auto j=entries.at(i);auto n=j["practice"]["name"].get<std::string>();
            if(ws->exists(n)||fs::exists(ws->logs(n))){wxTextEntryDialog d(panel,W("名稱已存在，請輸入新的練習名稱"),W("復原練習"),W(n));if(d.ShowModal()!=wxID_OK)return;n=U(d.GetValue());validName(n);}
            if(!closeProjectForChange())return;
            try{ws->restore(j["trashId"].get<std::string>(),n);loadProject();deferRender();}catch(...){loadProject();throw;}
        });
        button(panel,body,W("永久刪除選取練習"),[this,list,entries]{int i=list->GetSelection();if(i==wxNOT_FOUND)return;if(wxMessageBox(W("永久刪除此練習？此操作無法復原。"),W("永久刪除"),wxYES_NO|wxNO_DEFAULT|wxICON_WARNING,panel)==wxYES){ws->purge(entries.at(i)["trashId"].get<std::string>());deferRender();}});
        button(panel,body,W("清空垃圾桶"),[this,entries]{if(entries.empty())return;if(wxMessageBox(W("永久刪除垃圾桶內的所有練習？此操作無法復原。"),W("清空垃圾桶"),wxYES_NO|wxNO_DEFAULT|wxICON_WARNING,panel)==wxYES){for(auto& j:entries)ws->purge(j["trashId"].get<std::string>());deferRender();}});
    }
    void render(){
        if(!ui.beginRender()){renderPending=true;return;}
        struct Finish {UiLifecycle& ui;~Finish(){ui.endRender();}} finish{ui};
        trace("render: begin");
        // Deleting a selected native TreeView can emit a selection notification.
        // Disconnect it before invalidating the member and destroying controls.
        if(tree)tree->SetEvtHandlerEnabled(false);
        clockLabel=nullptr;timerButton=nullptr;finishButton=nullptr;tree=nullptr;body->Clear(true);
        if(!ws){label(panel,body,W("APCS 練習"));button(panel,body,W("開啟練習資料夾"),[this]{chooseOpen();});button(panel,body,W("新建練習資料夾"),[this]{createFolder();});button(panel,body,W("設定預設設定"),[this]{settingsDialog(true);});}
        else if(inTrash)renderTrash();
        else if(!current.empty()){
            button(panel,body,W("＜ 返回"),[this]{leavePractice();});
            auto& r=ws->row(current);label(panel,body,W(r["name"].get<std::string>()));clockLabel=new wxStaticText(panel,wxID_ANY,W("00:00:00"));clockLabel->SetFont(clockLabel->GetFont().Scale(1.6));body->Add(clockLabel,0,wxALL,12);
            if(r["state"]=="finished"){
                label(panel,body,W("已完成\n作答結果："+r.value("result",std::string())+"\n解題編號："+r.value("submission",std::string())));
                button(panel,body,W("恢復為未完成"),[this]{ws->reopen(current);deferRender();});button(panel,body,W("修改作答資料"),[this]{complete(true);});
            }else{
                timerButton=button(panel,body,W("開始"),[this]{auto& t=timer(current);if(t.running)t.pause();else t.start();t.save(*ws);deferRender();});
                finishButton=button(panel,body,W("完成練習"),[this]{complete();});
            }
            if(!r["history"].empty())button(panel,body,W("查看完成紀錄"),[this]{showHistory();});
            updateClock();
        }else{
            label(panel,body,W(str(ws->root.filename())));auto bar=new wxBoxSizer(wxHORIZONTAL);body->Add(bar,0,wxEXPAND);
            button(panel,bar,W("新增練習"),[this]{addPractice();});button(panel,bar,W("刪除練習"),[this]{deletePractice();});
            if(ws->settings["showTrash"].get<bool>())button(panel,bar,W("垃圾桶"),[this]{inTrash=true;deferRender();});
            button(panel,bar,W("設定"),[this]{settingsDialog(false);});
            tree=new wxTreeCtrl(panel,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxTR_HIDE_ROOT|wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT|wxTR_SINGLE);auto root=tree->AddRoot("root");undone=tree->AppendItem(root,W("未完成"));done=tree->AppendItem(root,W("已完成"));
            std::vector<json> rows;for(auto& r:ws->state["practices"])rows.push_back(r);
            auto sort=ws->settings["sort"].get<std::string>();std::stable_sort(rows.begin(),rows.end(),[&](auto& a,auto& b){if(sort=="name")return a["name"].template get<std::string>()<b["name"].template get<std::string>();auto aa=a.value("createdAt",std::string()),bb=b.value("createdAt",std::string());return sort=="oldest"?aa<bb:aa>bb;});
            for(auto& r:rows){auto k=r["id"].get<std::string>();auto text=r["name"].get<std::string>();auto& t=timer(k);if(ws->settings["showTime"].get<bool>())text+="  "+(t.known?duration(t.elapsed()):"時間未知");if(t.running)text+=" ▶";if(r["state"]=="finished"&&!r.value("result",std::string()).empty())text+="  "+r["result"].get<std::string>();tree->AppendItem(r["state"]=="finished"?done:undone,W(text),-1,-1,new PracticeData(k));}
            if(!ws->settings["rememberGroups"].get<bool>()||ws->settings["unfinishedExpanded"].get<bool>())tree->Expand(undone);
            if(!ws->settings["rememberGroups"].get<bool>()||ws->settings["finishedExpanded"].get<bool>())tree->Expand(done);
            auto sourceTree=tree;auto epoch=ui.epoch();
            tree->Bind(wxEVT_TREE_SEL_CHANGED,[this,sourceTree,epoch](wxTreeEvent& e){
                if(!ui.accepts(epoch)||tree!=sourceTree||!e.GetItem().IsOk())return;
                auto data=dynamic_cast<PracticeData*>(sourceTree->GetItemData(e.GetItem()));
                if(data){auto k=data->key;postAction([this,k]{if(ws)enter(k);},epoch);}
            });
            auto group=[this,sourceTree,epoch](wxTreeEvent& e){if(!ui.accepts(epoch)||tree!=sourceTree)return;if(ws&&ws->settings["rememberGroups"].get<bool>()){if(e.GetItem()==undone)ws->settings["unfinishedExpanded"]=e.GetEventType()==wxEVT_TREE_ITEM_EXPANDED;else if(e.GetItem()==done)ws->settings["finishedExpanded"]=e.GetEventType()==wxEVT_TREE_ITEM_EXPANDED;guard([this]{ws->saveSettings();});}e.Skip();};tree->Bind(wxEVT_TREE_ITEM_EXPANDED,group);tree->Bind(wxEVT_TREE_ITEM_COLLAPSED,group);
            body->Add(tree,1,wxALL|wxEXPAND,6);button(panel,body,W("關閉練習資料夾"),[this]{if(closeWorkspace())deferRender();});
        }
        panel->Layout();
        trace("render: done");
    }
    void updateClock(){if(!ws||current.empty()||!clockLabel)return;auto& t=timer(current);clockLabel->SetLabel(W(t.known?duration(t.elapsed()):"時間未知"));if(timerButton)timerButton->SetLabel(W(t.running?"暫停":t.started?"繼續":"開始"));if(finishButton)finishButton->Show(t.started);}
    void tick(wxTimerEvent&){
        if(shutting)return;
        if(ui.busy()||syncing||refreshing)return;
        if(renderPending)deferRender();
        if(syncRetry)queueProjectSync();
        if(!ws)return;
        guard([this]{
            LASTINPUTINFO li={sizeof(li),0};DWORD idle=GetLastInputInfo(&li)?GetTickCount()-li.dwTime:0;int threshold=ws->settings["idleMinutes"].get<int>();
            DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);bool blur=pid!=GetCurrentProcessId();
            bool changed=false;for(auto& v:timers)if(v.second.running){
                if(threshold>0&&idle>=(DWORD)threshold*60000){v.second.pause(idle);v.second.save(*ws);changed=true;}
                else if(ws->settings["pauseOnBlur"].get<bool>()&&blur){v.second.pause();v.second.save(*ws);changed=true;}
            }
            if(++ticks%5==0)saveTimers();
            updateClock();if(changed)deferRender();
        });
    }
    void onProjectClose(CodeBlocksEvent& e){
        if(ws&&e.GetProject()&&samePath(P(e.GetProject()->GetFilename()),ws->cbp()))prepareHiddenProjectTree();
        if(ui.busy()){syncRetry=true;e.Skip();return;}
        if(!refreshing&&ws&&e.GetProject()&&samePath(P(e.GetProject()->GetFilename()),ws->cbp()))guard([this]{releaseWorkspace();});
        e.Skip();queueProjectSync();
    }
    void onProjectChanged(CodeBlocksEvent& e){e.Skip();queueProjectSync();}
    void onBuild(CodeBlocksEvent& e){if(ws&&e.GetProject()==project())guard([this]{if(!Manager::Get()->GetEditorManager()->SaveAll())throw std::runtime_error("Save cancelled; build snapshot will use the last saved file.");});e.Skip();}
    void onStartup(CodeBlocksEvent& e){e.Skip();CallAfter([this]{if(shutting)return;guard([this]{
        // The IDE's restored/current project takes priority over the remembered folder.
        if(Manager::Get()->GetProjectManager()->GetActiveProject()){syncActiveProject();return;}
        if(!ws&&cfg()->ReadBool("reopen_last",true)){auto p=cfg()->Read("last_folder");if(!p.empty()&&fs::is_directory(P(p)))runAction([this,p]{openFolder(P(p));});}
    });});}
    void onShutdown(CodeBlocksEvent& e){shutting=true;ticker.Stop();if(ws){try{prepareHiddenProjectTree();pauseAll();remember();}catch(const std::exception& ex){wxMessageBox(W(ex.what()),W("APCS 計時儲存失敗"),wxOK|wxICON_ERROR,panel);}}e.Skip();}
protected:
    void OnAttach()override{
        globalDefaults=defaults();try{auto s=cfg()->Read("defaults");if(!s.empty())globalDefaults=normalizeSettings(json::parse(U(s)));}catch(...){wxMessageBox(W("插件預設設定損壞，暫時使用內建預設；原設定尚未覆寫。"),W("APCS 練習"));}
        panel=new wxPanel(Manager::Get()->GetAppWindow());body=new wxBoxSizer(wxVERTICAL);panel->SetSizer(body);render();
        CodeBlocksDockEvent dock(cbEVT_ADD_DOCK_WINDOW);dock.name="APCSPracticeStudio";dock.title=W("APCS 練習");dock.pWindow=panel;dock.dockSide=CodeBlocksDockEvent::dsRight;dock.desiredSize=wxSize(430,580);dock.minimumSize=wxSize(340,300);dock.shown=true;Manager::Get()->ProcessEvent(dock);
        ticker.SetOwner(this);Bind(wxEVT_TIMER,&APCSPracticeStudio::tick,this);ticker.Start(1000);
        auto m=Manager::Get();m->RegisterEventSink(cbEVT_PROJECT_CLOSE,new cbEventFunctor<APCSPracticeStudio,CodeBlocksEvent>(this,&APCSPracticeStudio::onProjectClose));
        m->RegisterEventSink(cbEVT_PROJECT_OPEN,new cbEventFunctor<APCSPracticeStudio,CodeBlocksEvent>(this,&APCSPracticeStudio::onProjectChanged));
        m->RegisterEventSink(cbEVT_PROJECT_ACTIVATE,new cbEventFunctor<APCSPracticeStudio,CodeBlocksEvent>(this,&APCSPracticeStudio::onProjectChanged));
        m->RegisterEventSink(cbEVT_COMPILER_FINISHED,new cbEventFunctor<APCSPracticeStudio,CodeBlocksEvent>(this,&APCSPracticeStudio::onProjectChanged));
        m->RegisterEventSink(cbEVT_COMPILER_STARTED,new cbEventFunctor<APCSPracticeStudio,CodeBlocksEvent>(this,&APCSPracticeStudio::onBuild));
        m->RegisterEventSink(cbEVT_APP_STARTUP_DONE,new cbEventFunctor<APCSPracticeStudio,CodeBlocksEvent>(this,&APCSPracticeStudio::onStartup));
        m->RegisterEventSink(cbEVT_APP_START_SHUTDOWN,new cbEventFunctor<APCSPracticeStudio,CodeBlocksEvent>(this,&APCSPracticeStudio::onShutdown));
        queueProjectSync(); // Also detect projects opened before the plugin was enabled.
    }
    void OnRelease(bool shutdown)override{
        shutting=true;ticker.Stop();if(ws){try{pauseAll();}catch(...){}ws.reset();}
        if(!shutdown){Manager::Get()->RemoveAllEventSinksFor(this);if(panel){CodeBlocksDockEvent e(cbEVT_REMOVE_DOCK_WINDOW);e.pWindow=panel;Manager::Get()->ProcessEvent(e);panel->Destroy();}}
        panel=nullptr;
    }
public:
    void BuildMenu(wxMenuBar* bar)override{
        int idx=bar->FindMenu("APCS");wxMenu* m;if(idx==wxNOT_FOUND){m=new wxMenu;bar->Append(m,"APCS");}else m=bar->GetMenu(idx);
        if(!m->FindItem(menuId))m->Append(menuId,W("練習 Panel"));
        Bind(wxEVT_MENU,[this](wxCommandEvent&){if(panel){CodeBlocksDockEvent e(cbEVT_SHOW_DOCK_WINDOW);e.pWindow=panel;Manager::Get()->ProcessEvent(e);queueProjectSync();}},menuId);
    }
};
namespace { PluginRegistrant<APCSPracticeStudio> registration("APCSPracticeStudio"); }
