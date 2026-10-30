# APCS Practice Studio

在 **Code::Blocks** 裡管理 C++／APCS 練習的原生插件：建立題目、切換編譯目標、計時、保存編譯時的程式快照，以及記錄完成狀態。

目前版本：**1.1.6**。已在 **Windows x64、Code::Blocks 25.03、MinGW GCC 14.2、wxWidgets 3.2.7／SDK 2.25.0** 環境建置與測試。預編譯安裝包針對這組環境；其他版本與平台尚未驗證。

## 下載與安裝

**[下載 APCSPracticeStudio.cbplugin](https://github.com/xHim1777/Code-Blocks-APCS-Practice-plugin/raw/refs/heads/main/dist/APCSPracticeStudio.cbplugin)**

一般使用者只需要這個安裝包，不必下載原始碼、SDK 或自行編譯。

1. 儲存 Code::Blocks 裡正在編輯的檔案。
2. 開啟 **Plugins → Manage plugins → Install new**。
3. 選擇下載的 `APCSPracticeStudio.cbplugin`；已有舊版時依 IDE 提示更新，完成後重新啟動 Code::Blocks。
4. 從 **APCS → 練習 Panel** 顯示面板。
5. 確認有可用的 MinGW `g++.exe`。插件會嘗試自動偵測，也可在設定裡手動選擇。

安裝包包含編譯紀錄工具；Code::Blocks 與 C++ 編譯器仍須另外安裝。可用 [dist/SHA256SUMS.txt](dist/SHA256SUMS.txt) 核對安裝包。

## 開始練習

1. 在 Panel 選擇「新建練習資料夾」，選取儲存位置並自行命名；或「開啟練習資料夾」載入既有資料。
2. 按「新增練習」輸入名稱，例如 `b266-矩陣翻轉`。名稱不可重複，也不可含 `.`、路徑分隔符或 Windows 保留名稱等不適用字元。
3. 點選練習，插件會開啟該題 CPP，並同步 Code::Blocks 的專案及 Build target。新增練習後會重新載入專案，讓新增 target 生效。
4. 按「開始」計時；計時中可「暫停」，暫停後可「繼續」。使用 Code::Blocks 原本的 Build／Run 功能編譯及執行。
5. 開始過計時後，可按「完成練習」，在確認視窗填寫結果（例如 AC、NA）及解題編號。兩個欄位都可留空。
6. 已完成練習可恢復為未完成、繼續累計時間，也可修改作答資料及查看完成歷史。

返回列表會暫停計時。預設同時關閉該題的 CPP 分頁，保留 Code::Blocks；有未儲存修改時由 IDE 詢問，取消則留在練習頁。可在設定關閉自動關分頁。

## 功能

- **專案同步**：從插件開啟資料夾時自動開啟對應 `.cbp`；從 IDE 開啟本插件的專案時 Panel 自動辨識。一般 Code::Blocks 專案不會被轉換成練習資料夾。
- **分類列表**：分為未完成／已完成，可依建立時間或名稱排序，選擇是否顯示時間及記住展開狀態。
- **獨立計時**：每題分別保存累計時間，暫停及關閉期間不累計；載入後預設暫停，進入時是否自動開始由設定決定。約每五秒及重要操作時儲存。
- **編譯紀錄**：每次編譯保存 HTML，包含開始時間（台灣時間）、當次 CPP 原始碼、SHA-256、編譯指令、編譯器輸出及結束代碼。即使程式沒改，另一次編譯也有獨立 log。
- **垃圾桶**：預設刪除會移至插件自己的垃圾桶，可復原程式、產物、log、時間與狀態；同名衝突時須另取名稱。也支援永久刪除及清空垃圾桶。
- **資料保護**：工作區鎖避免同時寫入；缺少必要檔案時先詢問是否補建；損壞 JSON 可在有可用備份時確認還原。無法確認的計時顯示未知，不當成零。

**「完成練習」是練習狀態，不代表 ZeroJudge 已 AC。** 結果及解題編號由使用者填寫，插件不登入、提交或驗證 ZeroJudge，也不自動同步 Notion。編譯成功不代表答案正確。

## 設定

首頁的「設定預設設定」用於之後新建的練習資料夾；已開啟資料夾的「設定」只影響該資料夾。

| 類型 | 可調整項目 |
| --- | --- |
| 計時 | 進入時自動開始、切換時暫停、失焦暫停、閒置自動暫停（0 為關閉） |
| 列表 | 排序、顯示時間、記住分類展開狀態、新增後自動進入 |
| 編譯 | C++11／14／17／20／23、`-Wall -Wextra`、編譯器路徑、CPP 模板 |
| 行為 | 啟動時重開上次資料夾、返回時關閉該題 CPP 分頁 |
| 刪除 | 顯示垃圾桶、刪除時移至垃圾桶（預設開啟） |

每次編譯儲存 log 是固定行為。閒置判斷依整台電腦的鍵盤／滑鼠活動；達門檻時扣除該次計時區段內的閒置時間，不扣先前累計。

## 資料儲存

練習資料夾本身就是可開啟的 Code::Blocks 專案，名稱與位置由使用者決定。

```text
練習資料夾/
├─ APCSPractice.cbp
├─ setting.json
├─ allPracticeState.json
├─ allPracticeTime.json
├─ allPractice/
│  └─ 練習名稱/
│     ├─ 練習名稱.cpp
│     ├─ 練習名稱.o
│     └─ 練習名稱.exe
├─ allLogs/
│  └─ 練習名稱/*.html
├─ .apcs/                    # 編譯工具與診斷紀錄
├─ .trash/                   # 插件垃圾桶（不是 Windows 資源回收筒）
└─ .apcs-workspace.lock
```

### 供其他工具讀取

- `allPracticeState.json`：`practices[]` 各筆有 `id`、`name`、`state`（`unfinished`／`finished`）、`createdAt`、`history`；完成後有 `result`、`submission`。`history` 保存完成當時的時間與作答資料。
- `allPracticeTime.json`：以 `practices[id]` 對應同一題；`elapsedMs` 為累計毫秒數，另有 `started`、`running`、`timeKnown`、`savedAt`。`elapsedMs=null` 或 `timeKnown=false` 表示未知。
- 以 `name` 對應 CPP／log 資料夾，以 `id` 連結狀態與計時。`history[].elapsedMs` 是每次完成時的累計快照，不能逐筆相加。
- `savedAt` 是儲存時間；外部工具應讀取已儲存的累計值，不能用現在時間減 `savedAt` 補算。JSON 時間使用 UTC，HTML log 的 Started 有 `+08:00`。
- log 含完整程式碼，分享前請確認內容。備份時建議先暫停並關閉練習專案，再複製整個練習資料夾；不要在插件執行中手動改寫 JSON。

## 從原始碼建置

一般安裝不需要這個步驟。開發環境需 Windows x64、**PowerShell 7**、與目標 IDE 相符的 MinGW、Code::Blocks SDK 及 wxWidgets 標頭／產生的設定標頭。

本儲存庫包含 `nlohmann/json` 單一標頭，但**未包含完整 SDK 或 wxWidgets 開發套件**。目前建置腳本的 `-Dependencies` 參數預期下列目錄配置：

```text
dependencies/
├─ include/wx/                            # wxWidgets 標頭
├─ lib/gcc_dll/mswu/wx/setup.h             # 對應 Unicode DLL 的設定標頭
└─ sdk-source/codeblocks_25.03/src/
   ├─ include/                           # 含 tinyxml 標頭
   └─ sdk/wxscintilla/include/
```

準備與目標 IDE 相符的開發檔案後，在儲存庫根目錄用 PowerShell 7 執行：

```powershell
./build.ps1 -CodeBlocks 'C:\Program Files\CodeBlocks' -Dependencies 'D:\dependencies'
./tests/integration.ps1
```

`-CodeBlocks` 預設為 `C:\Program Files\CodeBlocks`；`-Dependencies` 預設為儲存庫旁的 `apcs-plugin` 資料夾。上面的配置是外部依賴位置，與舊插件的程式碼無關。建置腳本唯讀使用這些開發檔案，並從已安裝的 `codeblocks.dll`、`wxmsw32u_gcc_custom.dll` 產生匯入函式庫。

建置會編譯 helper、執行核心測試、編譯 DLL，再驗證並產生 `dist/APCSPracticeStudio.cbplugin`。`tests/integration.ps1` 使用前一步產生的隔離測試資料，驗證實際 C++ 編譯、執行、log 快照及清理；目前測試腳本的編譯器路徑固定為預設安裝位置，使用其他位置時需先調整該測試設定。測試產物位於 `build/`，不包含在儲存庫內。

## 1.1.6 更新與驗證

- 修正進入另一題後仍使用舊 Build target：同步專案、工作區偏好目標及 CompilerGCC 工具列。
- 避開 Code::Blocks 25.03 在 Management 隱藏時，關閉專案／IDE 重建專案樹造成的 `GetNextVisible`／`IsVisible` 斷言。
- 限制閒置扣時只扣當次計時區段，避免減少之前累計。

此版 **67 項核心檢查**與編譯／執行／HTML log 整合測試通過；已在真實 IDE 驗證切換兩題後分別編譯正確目標，以及關閉資料夾／IDE 未再出現上述斷言。強制終止、斷電、所有設定組合及其他 IDE 版本尚未完整驗證。

## 授權與第三方元件

本專案原始碼採 [MIT License](LICENSE)。第三方元件及外部連結依賴保留各自授權，見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
