#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <mupdf/fitz.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#define WM_USER_PROG_MAX  (WM_USER + 1)
#define WM_USER_PROG_VAL  (WM_USER + 2)
#define WM_USER_STATUS    (WM_USER + 3) 
#define WM_USER_DONE      (WM_USER + 4)

#define IDC_LISTBOX   101
#define IDC_BTN_CLEAR 102
#define IDC_RDO_PNG   103
#define IDC_RDO_JPG   104
#define IDC_CMB_QUAL  105
#define IDC_EDT_PATH  106
#define IDC_BTN_BROWSE 107
#define IDC_BTN_START 108
#define IDC_PROGRESS  109
#define IDC_LBL_STATUS 110

HWND hList, hBtnClear, hRdoPng, hRdoJpg, hCmbQual, hEdtPath, hBtnBrowse, hBtnStart, hProg, hLblStatus;

// 【修复1】：声明全局字体句柄，便于在程序退出时释放
HFONT hGlobalFont = NULL; 

std::vector<std::wstring> pdf_files;
std::atomic<bool> is_running(false);

std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, NULL, NULL);
    return result;
}

void ConvertThread(HWND hWnd, std::vector<std::wstring> files, std::wstring out_dir, bool is_jpg, float zoom) {
    auto start_time = std::chrono::steady_clock::now();
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(ctx);
    int total_files = (int)files.size();

    for (int i = 0; i < total_files; ++i) {
        if (!is_running) break;
        std::wstring w_pdf_path = files[i];
        std::string utf8_pdf_path = WStringToString(w_pdf_path);

        size_t slash_pos = w_pdf_path.find_last_of(L"/\\");
        std::wstring base_name = (slash_pos == std::wstring::npos) ? w_pdf_path : w_pdf_path.substr(slash_pos + 1);
        size_t dot_pos = base_name.find_last_of(L".");
        if (dot_pos != std::wstring::npos) base_name = base_name.substr(0, dot_pos);

        std::wstring current_out_dir = out_dir;
        if (current_out_dir.empty()) {
            current_out_dir = (slash_pos == std::wstring::npos) ? L"." : w_pdf_path.substr(0, slash_pos);
        }

        std::wstring status_text = L"[" + std::to_wstring(i + 1) + L"/" + std::to_wstring(total_files) + L"] 正在转换: " + base_name;
        
        // 【修复2】：利用我们自定义的 WM_USER_STATUS 发送给主窗口，保证跨线程 UI 更新安全
        SendMessage(hWnd, WM_USER_STATUS, 0, (LPARAM)status_text.c_str());

        fz_try(ctx) {
            fz_document *doc = fz_open_document(ctx, utf8_pdf_path.c_str());
            int page_count = fz_count_pages(ctx, doc);
            PostMessage(hWnd, WM_USER_PROG_MAX, page_count, 0);
            PostMessage(hWnd, WM_USER_PROG_VAL, 0, 0);

            for (int p = 0; p < page_count; ++p) {
                if (!is_running) break;
                fz_page *page = fz_load_page(ctx, doc, p);
                fz_matrix mat = fz_scale(zoom, zoom);
                
                fz_pixmap *pix = fz_new_pixmap_from_page_contents(ctx, page, mat, fz_device_rgb(ctx), 0);

                wchar_t page_num[10];
                swprintf(page_num, 10, L"%03d", p + 1);
                std::wstring ext = is_jpg ? L".jpg" : L".png";
                std::wstring w_out_file = current_out_dir + L"\\page" + page_num + L"_" + base_name + ext;
                std::string utf8_out_file = WStringToString(w_out_file);

                if (is_jpg) fz_save_pixmap_as_jpeg(ctx, pix, utf8_out_file.c_str(), 90);
                else fz_save_pixmap_as_png(ctx, pix, utf8_out_file.c_str());

                fz_drop_pixmap(ctx, pix);
                fz_drop_page(ctx, page);
                PostMessage(hWnd, WM_USER_PROG_VAL, p + 1, 0);
            }
            fz_drop_document(ctx, doc);
        } fz_catch(ctx) {}
    }
    fz_drop_context(ctx);

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time; 

    is_running = false;
    
    // 【修改这里】：使用 swprintf 格式化为保留 2 位小数
    wchar_t time_buf[100];
    swprintf(time_buf, 100, L"转换完成！耗时: %.2f 秒", elapsed.count());
    std::wstring done_msg = time_buf;

    SendMessage(hWnd, WM_USER_STATUS, 0, (LPARAM)done_msg.c_str()); 
    PostMessage(hWnd, WM_USER_DONE, 0, 0);
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
    SendMessage(hwnd, WM_SETFONT, lParam, TRUE);
    return TRUE;
}

// 【修复3】：增加辅助函数，用于转换时禁用/恢复其他UI控件
void EnableControls(BOOL enable) {
    EnableWindow(hBtnClear, enable);
    EnableWindow(hRdoPng, enable);
    EnableWindow(hRdoJpg, enable);
    EnableWindow(hCmbQual, enable);
    EnableWindow(hEdtPath, enable);
    EnableWindow(hBtnBrowse, enable);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindow(L"STATIC", L"👇 拖拽 PDF 到此处 👇", WS_VISIBLE | WS_CHILD | SS_CENTER, 250, 15, 200, 20, hWnd, NULL, NULL, NULL);
        hList = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_EXTENDEDSEL, 20, 45, 660, 150, hWnd, (HMENU)IDC_LISTBOX, NULL, NULL);
        hBtnClear = CreateWindow(L"BUTTON", L"清空列表", WS_VISIBLE | WS_CHILD, 20, 205, 100, 30, hWnd, (HMENU)IDC_BTN_CLEAR, NULL, NULL);

        CreateWindow(L"STATIC", L"输出格式:", WS_VISIBLE | WS_CHILD, 20, 255, 70, 20, hWnd, NULL, NULL, NULL);
        hRdoPng = CreateWindow(L"BUTTON", L"PNG", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, 100, 250, 60, 20, hWnd, (HMENU)IDC_RDO_PNG, NULL, NULL);
        hRdoJpg = CreateWindow(L"BUTTON", L"JPG", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 170, 250, 60, 20, hWnd, (HMENU)IDC_RDO_JPG, NULL, NULL);
        SendMessage(hRdoPng, BM_SETCHECK, BST_CHECKED, 0);

        CreateWindow(L"STATIC", L"清晰度:", WS_VISIBLE | WS_CHILD, 260, 255, 60, 20, hWnd, NULL, NULL, NULL);
        hCmbQual = CreateWindow(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, 320, 252, 150, 150, hWnd, (HMENU)IDC_CMB_QUAL, NULL, NULL);
        SendMessage(hCmbQual, CB_ADDSTRING, 0, (LPARAM)L"极速 (1x)");
        SendMessage(hCmbQual, CB_ADDSTRING, 0, (LPARAM)L"标准 (2x) - 推荐");
        SendMessage(hCmbQual, CB_ADDSTRING, 0, (LPARAM)L"高清 (3x)");
        SendMessage(hCmbQual, CB_SETCURSEL, 1, 0);

        CreateWindow(L"STATIC", L"保存路径 (留空则在原目录):", WS_VISIBLE | WS_CHILD, 20, 305, 200, 20, hWnd, NULL, NULL, NULL);
        hEdtPath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 20, 330, 550, 25, hWnd, (HMENU)IDC_EDT_PATH, NULL, NULL);
        hBtnBrowse = CreateWindow(L"BUTTON", L"浏览...", WS_VISIBLE | WS_CHILD, 580, 328, 80, 28, hWnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);

        hLblStatus = CreateWindow(L"STATIC", L"准备就绪", WS_VISIBLE | WS_CHILD, 20, 385, 660, 20, hWnd, (HMENU)IDC_LBL_STATUS, NULL, NULL);
        hProg = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 20, 415, 660, 25, hWnd, (HMENU)IDC_PROGRESS, NULL, NULL);
        hBtnStart = CreateWindow(L"BUTTON", L"🚀 开始转换", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 250, 465, 200, 40, hWnd, (HMENU)IDC_BTN_START, NULL, NULL);

        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
        
        // 【修复1】：赋值给全局变量
        hGlobalFont = CreateFontIndirect(&ncm.lfMessageFont);
        EnumChildWindows(hWnd, EnumChildProc, (LPARAM)hGlobalFont);

        DragAcceptFiles(hWnd, TRUE);
        break;
    }
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < fileCount; i++) {
            wchar_t filePath[MAX_PATH];
            DragQueryFile(hDrop, i, filePath, MAX_PATH);
            std::wstring pathStr = filePath;
            if (pathStr.length() > 4 && pathStr.substr(pathStr.length() - 4) == L".pdf") {
                bool exists = false;
                for (const auto& p : pdf_files) {
                    if (p == pathStr) { exists = true; break; }
                }
                if (!exists) {
                    pdf_files.push_back(pathStr);
                    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)pathStr.c_str());
                }
            }
        }
        DragFinish(hDrop);
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_BTN_CLEAR) {
            pdf_files.clear();
            SendMessage(hList, LB_RESETCONTENT, 0, 0);
        }
        else if (wmId == IDC_BTN_BROWSE) {
            BROWSEINFO bi = { 0 };
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"请选择保存目录";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
            if (pidl != 0) {
                wchar_t path[MAX_PATH];
                if (SHGetPathFromIDList(pidl, path)) {
                    SetWindowText(hEdtPath, path);
                }
                CoTaskMemFree(pidl);
            }
        }
        else if (wmId == IDC_BTN_START) {
            if (pdf_files.empty()) {
                MessageBox(hWnd, L"请先拖入 PDF 文件！", L"提示", MB_ICONWARNING);
                break;
            }
            if (is_running) break;

            bool is_jpg = (SendMessage(hRdoJpg, BM_GETCHECK, 0, 0) == BST_CHECKED);
            int qual_idx = SendMessage(hCmbQual, CB_GETCURSEL, 0, 0);
            float zoom = (qual_idx == 0) ? 1.0f : (qual_idx == 2 ? 3.0f : 2.0f);

            wchar_t path_buf[MAX_PATH];
            GetWindowText(hEdtPath, path_buf, MAX_PATH);
            std::wstring out_dir = path_buf;

            is_running = true;
            EnableWindow(hBtnStart, FALSE);
            EnableControls(FALSE); // 【修复3】：转换期间禁用其他控件，防止用户乱点
            SetWindowText(hBtnStart, L"正在处理...");
            
            std::thread(ConvertThread, hWnd, pdf_files, out_dir, is_jpg, zoom).detach();
        }
        break;
    }
    case WM_USER_PROG_MAX:
        SendMessage(hProg, PBM_SETRANGE32, 0, wParam);
        break;
    case WM_USER_PROG_VAL:
        SendMessage(hProg, PBM_SETPOS, wParam, 0);
        break;
    case WM_USER_STATUS:
        SetWindowText(hLblStatus, (LPCWSTR)lParam);
        break;
    case WM_USER_DONE:
        EnableWindow(hBtnStart, TRUE);
        EnableControls(TRUE); // 【修复3】：转换完成恢复其他控件
        SetWindowText(hBtnStart, L"🚀 开始转换");
        SendMessage(hProg, PBM_SETPOS, 0, 0);
        MessageBox(hWnd, L"转换完成！", L"成功", MB_OK | MB_ICONINFORMATION);
        break;

    case WM_DESTROY:
        is_running = false;
        if (hGlobalFont) DeleteObject(hGlobalFont); // 【修复1】：清理字体资源
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    CoInitialize(NULL);

    LPCWSTR myWndClass = L"PdfToImageAppClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = myWndClass;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_ACCEPTFILES, myWndClass, L"PDF 转图片 (C++ 硬核重写版)", 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 560, NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}