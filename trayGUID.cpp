#define _WIN32_WINNT 0x0500 // Target Windows 2000 or later
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// g++ TrayGuid.cpp -o TrayGuid.exe -DUNICODE -D_UNICODE -lgdi32 -luser32 -lkernel32 -lole32 -lshell32 -mwindows

// Link against these libraries
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// Constants
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 1001
#define ID_HOTKEY 2001

// Menu IDs
#define ID_MENU_EXIT 3001
#define ID_MENU_RELOAD 3002

// Global Variables
NOTIFYICONDATA nid;
HWND hMainWnd;
UINT vkKey = 'G'; // Default Key
UINT fsModifiers = MOD_CONTROL | MOD_ALT; // Default Modifiers

// --- Helper: Generate GUID String ---
std::wstring GenerateGUID() {
    GUID guid;
    HRESULT hCreate = CoCreateGuid(&guid);
    
    if (FAILED(hCreate)) return L"ERROR-GUID-FAIL";

    wchar_t szGuid[40] = {0};
    StringFromGUID2(guid, szGuid, 40);
    
    // StringFromGUID2 returns {XXXX...}. We often want just the numbers.
    // Let's strip the braces {} if you prefer raw format.
    std::wstring result = szGuid;
    if (result.size() > 2) {
        result = result.substr(1, result.size() - 2);
    }
    
    for (auto & c : result) {
        c = towlower(c);
    }

    return result;

}

// --- Helper: Simulate Typing ---
void TypeString(const std::wstring& str) {
    std::vector<INPUT> inputs;
    for (wchar_t c : str) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = c;
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(input); // Key Down

        input.ki.dwFlags |= KEYEVENTF_KEYUP;
        inputs.push_back(input); // Key Up
    }
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

// --- Helper: Load Config from INI ---
void LoadConfig() {
    UnregisterHotKey(hMainWnd, ID_HOTKEY);

    // Simple parsing of a local file named "config.ini"
    // Format expected: 
    // MOD=3 (Note: 1=Alt, 2=Ctrl, 4=Shift, 8=Win)
    // KEY=71 (ASCII/Virtual Key code, 71 is 'G')
    
    std::ifstream infile("config.ini");
    if (infile.good()) {
        std::string line;
        while (std::getline(infile, line)) {
            if (line.find("MOD=") == 0) {
                fsModifiers = std::stoi(line.substr(4));
            }
            if (line.find("KEY=") == 0) {
                vkKey = std::stoi(line.substr(4));
            }
        }
    }
    
    if (!RegisterHotKey(hMainWnd, ID_HOTKEY, fsModifiers, vkKey)) {
        MessageBox(NULL, L"Failed to register Hotkey! Check if another app is using it.", L"Error", MB_ICONERROR);
    }
}

// --- Window Procedure ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        // Initialize Tray Icon
        memset(&nid, 0, sizeof(NOTIFYICONDATA));
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hWnd;
        nid.uID = ID_TRAY_ICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Default App Icon
        lstrcpy(nid.szTip, L"GUID Typer (Right Click to Configure)");
        Shell_NotifyIcon(NIM_ADD, &nid);
        
        // Load settings and register hotkey
        hMainWnd = hWnd;
        LoadConfig();
        break;

    case WM_HOTKEY:
        if (wParam == ID_HOTKEY) {
            // Hotkey pressed! Generate and Type.
            std::wstring guid = GenerateGUID();
            TypeString(guid);
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_MENU_RELOAD, L"Reload Config (config.ini)");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_MENU_EXIT, L"Exit");

            SetForegroundWindow(hWnd); // Fix for menu not closing
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_MENU_EXIT) {
            DestroyWindow(hWnd);
        }
        else if (LOWORD(wParam) == ID_MENU_RELOAD) {
            LoadConfig();
            MessageBox(hWnd, L"Configuration Reloaded!", L"Info", MB_OK);
        }
        break;

    case WM_DESTROY:
        UnregisterHotKey(hWnd, ID_HOTKEY);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// --- Entry Point ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"TrayGuidAppClass";

    RegisterClassEx(&wcex);

    // Create a generic window (Hidden) to handle messages
    HWND hWnd = CreateWindow(L"TrayGuidAppClass", L"GUID Typer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd) return FALSE;

    // Standard Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}