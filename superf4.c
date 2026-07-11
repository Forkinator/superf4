/*
  Copyright (C) 2019  Stefan Sundin
  Copyright (C) 2026  Forkinator fork contributors

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0600
#define OEMRESOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlwapi.h>
#include <psapi.h>

// App
#define APP_NAME            L"SuperF4"
#define APP_VERSION         "1.4.2"
#define APP_URL             L"https://github.com/Forkinator/superf4"

// Messages
#define WM_TRAY                   WM_USER+1
#define SWM_TOGGLE                WM_APP+1
#define SWM_ELEVATE               WM_APP+2
#define SWM_AUTOSTART_ON          WM_APP+3
#define SWM_AUTOSTART_OFF         WM_APP+4
#define SWM_AUTOSTART_ELEVATE_ON  WM_APP+5
#define SWM_AUTOSTART_ELEVATE_OFF WM_APP+6
#define SWM_TIMERCHECK_ON         WM_APP+7
#define SWM_TIMERCHECK_OFF        WM_APP+8
#define SWM_SETTINGS              WM_APP+9
#define SWM_WEBSITE               WM_APP+10
#define SWM_XKILL                 WM_APP+11
#define SWM_EXIT                  WM_APP+12
#define CHECKTIMER                WM_APP+13
#define SWM_ALWAYS_ELEVATE_ON     WM_APP+14
#define SWM_ALWAYS_ELEVATE_OFF    WM_APP+15

#define TRAY_LEFT_XKILL  0
#define TRAY_LEFT_TOGGLE 1

// Boring stuff
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ENABLED() (keyhook)
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
HINSTANCE g_hinst = NULL;
HWND g_hwnd = NULL;
UINT WM_TASKBARCREATED = 0;
UINT WM_UPDATESETTINGS = 0;
wchar_t inipath[MAX_PATH];
int HookKeyboard();
int HookMouse();
int UnhookMouse();
int DisableMouse();
void LoadSettings();
void RestoreCursors();
void MaybeShowFirstRunTip();

// Cool stuff
HHOOK keyhook = NULL;
HHOOK mousehook = NULL;
int superkill = 0;
#define CHECKINTERVAL 50
int killing = 0;
int elevated = 0;
int show_kill_message = 0;
int always_elevate = 0;
int tray_left_click = TRAY_LEFT_XKILL;
int show_first_run_tip = 1;
HCURSOR killcur = NULL;
int killcur_applied = 0;

struct denylist {
  wchar_t *data;
  wchar_t **items;
  int length;
};
struct denylist ProcessDenylist = {NULL, NULL, 0};

// Always-on safety list (cannot be cleared via ini)
static const wchar_t *kBuiltinDeny[] = {
  L"explorer.exe",
  L"dwm.exe",
  L"csrss.exe",
  L"winlogon.exe",
  L"services.exe",
  L"lsass.exe",
  L"smss.exe",
  L"svchost.exe",
  L"taskmgr.exe",
  L"System",
  L"Registry",
  L"fontdrvhost.exe",
  L"sihost.exe",
  L"RuntimeBroker.exe",
  L"SearchHost.exe",
  L"StartMenuExperienceHost.exe",
  L"ShellExperienceHost.exe",
  L"ApplicationFrameHost.exe",
  L"SecurityHealthService.exe",
  L"MsMpEng.exe",
  L"ctfmon.exe",
};

// Include stuff
#include "localization/strings.h"
#include "include/error.c"
#include "include/hotkey.c"
#include "include/autostart.c"
#include "include/tray.c"

static const struct Hotkey kDefaultKillHotkey = {1, 1, 0, 0, VK_F4};   // Ctrl+Alt+F4
static const struct Hotkey kDefaultXkillHotkey = {0, 0, 0, 1, VK_F4}; // Win+F4
struct Hotkey kill_hotkey;
struct Hotkey xkill_hotkey;

static wchar_t *TrimWide(wchar_t *str) {
  while (*str == L' ' || *str == L'\t') {
    str++;
  }
  if (*str == L'\0') {
    return str;
  }
  wchar_t *end = str + wcslen(str) - 1;
  while (end > str && (*end == L' ' || *end == L'\t')) {
    end--;
  }
  end[1] = L'\0';
  return str;
}

void ClearProcessDenylist() {
  free(ProcessDenylist.data);
  free(ProcessDenylist.items);
  ProcessDenylist.data = NULL;
  ProcessDenylist.items = NULL;
  ProcessDenylist.length = 0;
}

void LoadProcessDenylist(const wchar_t *txt) {
  ClearProcessDenylist();
  if (txt == NULL || txt[0] == L'\0') {
    return;
  }

  size_t len = wcslen(txt);
  ProcessDenylist.data = malloc((len + 1) * sizeof(wchar_t));
  if (ProcessDenylist.data == NULL) {
    return;
  }
  wcscpy(ProcessDenylist.data, txt);

  int count = 1;
  for (size_t i = 0; i < len; i++) {
    if (ProcessDenylist.data[i] == L',') {
      count++;
    }
  }
  ProcessDenylist.items = malloc(count * sizeof(wchar_t*));
  if (ProcessDenylist.items == NULL) {
    ClearProcessDenylist();
    return;
  }

  wchar_t *pos = ProcessDenylist.data;
  while (pos != NULL) {
    wchar_t *name = pos;
    pos = wcschr(pos, L',');
    if (pos != NULL) {
      *pos = L'\0';
      pos++;
    }
    name = TrimWide(name);
    if (name[0] != L'\0') {
      ProcessDenylist.items[ProcessDenylist.length++] = name;
    }
  }
}

static int NameIsDenied(const wchar_t *name) {
  if (name == NULL || name[0] == L'\0') {
    return 0;
  }
  for (int i = 0; i < (int)ARRAY_SIZE(kBuiltinDeny); i++) {
    if (!_wcsicmp(name, kBuiltinDeny[i])) {
      return 1;
    }
  }
  for (int i = 0; i < ProcessDenylist.length; i++) {
    if (!_wcsicmp(name, ProcessDenylist.items[i])) {
      return 1;
    }
  }
  return 0;
}

void LoadSettings() {
  wchar_t txt[1000];

  KillTimer(g_hwnd, CHECKTIMER);
  GetPrivateProfileString(L"General", L"TimerCheck", L"0", txt, ARRAY_SIZE(txt), inipath);
  if (_wtoi(txt)) {
    SetTimer(g_hwnd, CHECKTIMER, CHECKINTERVAL, NULL);
  }

  GetPrivateProfileString(L"General", L"ShowKillMessage", L"0", txt, ARRAY_SIZE(txt), inipath);
  show_kill_message = _wtoi(txt) ? 1 : 0;

  GetPrivateProfileString(L"General", L"ProcessDenylist", L"", txt, ARRAY_SIZE(txt), inipath);
  LoadProcessDenylist(txt);

  GetPrivateProfileString(L"General", L"TrayLeftClick", L"xkill", txt, ARRAY_SIZE(txt), inipath);
  tray_left_click = (!_wcsicmp(txt, L"toggle")) ? TRAY_LEFT_TOGGLE : TRAY_LEFT_XKILL;

  GetPrivateProfileString(L"General", L"KillHotkey", L"Ctrl+Alt+F4", txt, ARRAY_SIZE(txt), inipath);
  ParseHotkey(txt, &kill_hotkey, &kDefaultKillHotkey);

  GetPrivateProfileString(L"General", L"XkillHotkey", L"Win+F4", txt, ARRAY_SIZE(txt), inipath);
  ParseHotkey(txt, &xkill_hotkey, &kDefaultXkillHotkey);

  GetPrivateProfileString(L"General", L"ShowFirstRunTip", L"1", txt, ARRAY_SIZE(txt), inipath);
  show_first_run_tip = _wtoi(txt) ? 1 : 0;

  GetPrivateProfileString(L"Advanced", L"AlwaysElevate", L"0", txt, ARRAY_SIZE(txt), inipath);
  always_elevate = _wtoi(txt) ? 1 : 0;
}

static int IsElevatedProcess(void) {
  HANDLE token = NULL;
  TOKEN_ELEVATION elevation;
  DWORD len = 0;
  int result = 0;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &token)) {
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &len)) {
      result = elevation.TokenIsElevated ? 1 : 0;
    }
    CloseHandle(token);
  }
  return result;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
  { // Keep the stack clean after initialization
    g_hinst = hInst;
    kill_hotkey = kDefaultKillHotkey;
    xkill_hotkey = kDefaultXkillHotkey;

    GetModuleFileName(NULL, inipath, ARRAY_SIZE(inipath));
    PathRemoveFileSpec(inipath);
    wcscat(inipath, L"\\"APP_NAME".ini");
    wchar_t txt[1000];

    char *argv[10];
    int argc = 1;
    argv[0] = szCmdLine;
    while ((argv[argc]=strchr(argv[argc-1],' ')) != NULL) {
      *argv[argc] = '\0';
      if (argc == ARRAY_SIZE(argv)) break;
      argv[argc++]++;
    }

    int elevate = 0;
    for (int i = 0; i < argc; i++) {
      if (!strcmp(argv[i],"-elevate") || !strcmp(argv[i],"-e")) {
        elevate = 1;
      }
    }

    elevated = IsElevatedProcess();
    WM_UPDATESETTINGS = RegisterWindowMessage(L"UpdateSettings");

    // AlwaysElevate (also applied before window creation)
    GetPrivateProfileString(L"Advanced", L"AlwaysElevate", L"0", txt, ARRAY_SIZE(txt), inipath);
    always_elevate = _wtoi(txt) ? 1 : 0;

    if (!elevated) {
      if (always_elevate) {
        elevate = 1;
      }
      if (elevate) {
        wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, ARRAY_SIZE(path));
        int ret = (INT_PTR) ShellExecute(NULL, L"runas", path, NULL, NULL, SW_SHOWNORMAL);
        if (ret > 32) {
          return 0;
        }
      }
    }

    if (FindWindow(APP_NAME, NULL) != NULL) {
      return 0;
    }

    WNDCLASSEX wnd = { sizeof(WNDCLASSEX), 0, WindowProc, 0, 0, hInst, NULL, NULL, NULL, NULL, APP_NAME, NULL };
    RegisterClassEx(&wnd);
    g_hwnd = CreateWindowEx(0, wnd.lpszClassName, APP_NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInst, NULL);

    LoadSettings();
    InitTray();
    UpdateTray();
    HookKeyboard();
    MaybeShowFirstRunTip();
  }

  MSG msg;
  while (GetMessage(&msg,NULL,0,0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}

void Kill(HWND hwnd) {
  if (killing || hwnd == NULL || hwnd == g_hwnd) {
    return;
  }

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0 || pid == GetCurrentProcessId()) {
    return;
  }

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  wchar_t name[256] = L"";
  if (process != NULL) {
    DWORD ret = GetProcessImageFileName(process, name, ARRAY_SIZE(name));
    CloseHandle(process);
    process = NULL;
    if (ret != 0) {
      PathStripPath(name);
      if (NameIsDenied(name)) {
        return;
      }
    }
  }

  killing = 1;

  process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
  if (process == NULL) {
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES tkp;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken)) {
      LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
      tkp.PrivilegeCount = 1;
      tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
      if (AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0) && GetLastError() == ERROR_SUCCESS) {
        process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        tkp.Privileges[0].Attributes = 0;
        AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
      }
      CloseHandle(hToken);
    }
  }

  if (process == NULL) {
    #ifdef DEBUG
    Error(L"OpenProcess()", L"Kill()", GetLastError());
    #endif
    return;
  }

  if (TerminateProcess(process, 1) == 0) {
    #ifdef DEBUG
    Error(L"TerminateProcess()", L"Kill()", GetLastError());
    #endif
    CloseHandle(process);
    return;
  }

  if (show_kill_message) {
    ShowKillMessage(name[0] ? name : L"unknown process");
  }

  CloseHandle(process);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION) {
    int vkey = ((PKBDLLHOOKSTRUCT)lParam)->vkCode;

    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      if (HotkeyMatch(&kill_hotkey, vkey)) {
        HWND hwnd = GetForegroundWindow();
        if (hwnd != NULL) {
          Kill(hwnd);
        }
        return 1;
      }
      if (HotkeyMatch(&xkill_hotkey, vkey)) {
        HookMouse();
        return 1;
      }
      if (vkey == VK_ESCAPE && superkill) {
        UnhookMouse();
        return 1;
      }
    }
    else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
      killing = 0;
    }
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

HWND WindowFromClickPoint(POINT pt) {
  HWND hwnd = WindowFromPoint(pt);
  if (hwnd == NULL) {
    hwnd = WindowFromPhysicalPoint(pt);
  }
  if (hwnd == NULL) {
    POINT cur;
    if (GetCursorPos(&cur)) {
      hwnd = WindowFromPoint(cur);
      if (hwnd == NULL) {
        hwnd = WindowFromPhysicalPoint(cur);
      }
    }
  }

  if (hwnd == NULL || hwnd == g_hwnd) {
    return NULL;
  }

  HWND root = GetAncestor(hwnd, GA_ROOT);
  return root ? root : hwnd;
}

void ApplyKillCursor() {
  if (killcur_applied) {
    return;
  }
  if (killcur == NULL) {
    killcur = LoadImage(g_hinst, L"kill", IMAGE_CURSOR, 0, 0, LR_DEFAULTCOLOR);
    if (killcur == NULL) {
      return;
    }
  }

  static const UINT ids[] = {
    OCR_NORMAL, OCR_IBEAM, OCR_WAIT, OCR_CROSS, OCR_UP, OCR_SIZENWSE,
    OCR_SIZENESW, OCR_SIZEWE, OCR_SIZENS, OCR_SIZEALL, OCR_NO, OCR_HAND,
    OCR_APPSTARTING
  };
  for (int i = 0; i < (int)ARRAY_SIZE(ids); i++) {
    HCURSOR copy = CopyCursor(killcur);
    if (copy != NULL) {
      SetSystemCursor(copy, ids[i]);
    }
  }
  killcur_applied = 1;
}

void RestoreCursors() {
  if (!killcur_applied) {
    return;
  }
  SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
  killcur_applied = 0;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION && superkill) {
    if (wParam == WM_LBUTTONDOWN) {
      POINT pt = ((PMSLLHOOKSTRUCT)lParam)->pt;
      HWND hwnd = WindowFromClickPoint(pt);
      UnhookMouse();
      if (hwnd != NULL) {
        Kill(hwnd);
      }
      return 1;
    }
    else if (wParam == WM_RBUTTONDOWN) {
      DisableMouse();
      return 1;
    }
    else if (wParam == WM_RBUTTONUP) {
      UnhookMouse();
      return 1;
    }
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int HookMouse() {
  if (mousehook) {
    return 1;
  }

  mousehook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, g_hinst, 0);
  if (mousehook == NULL) {
    #ifdef DEBUG
    Error(L"SetWindowsHookEx(WH_MOUSE_LL)", L"HookMouse()", GetLastError());
    #endif
    return 1;
  }

  ApplyKillCursor();
  superkill = 1;
  return 0;
}

DWORD WINAPI DelayedUnhookMouse(LPVOID param) {
  HHOOK hook = (HHOOK)param;
  Sleep(100);
  if (hook != NULL) {
    UnhookWindowsHookEx(hook);
  }
  return 0;
}

int UnhookMouse() {
  if (!mousehook) {
    DisableMouse();
    return 1;
  }

  HHOOK hook = mousehook;
  mousehook = NULL;
  DisableMouse();

  HANDLE thread = CreateThread(NULL, 0, DelayedUnhookMouse, hook, 0, NULL);
  if (thread) {
    CloseHandle(thread);
  }
  else {
    UnhookWindowsHookEx(hook);
  }
  return 0;
}

int DisableMouse() {
  superkill = 0;
  killing = 0;
  RestoreCursors();
  return 0;
}

int HookKeyboard() {
  if (keyhook) {
    return 1;
  }

  keyhook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hinst, 0);
  if (keyhook == NULL) {
    Error(L"SetWindowsHookEx(WH_KEYBOARD_LL)", L"Could not hook keyboard. Another program might be interfering.", GetLastError());
    return 1;
  }

  UpdateTray();
  return 0;
}

int UnhookKeyboard() {
  if (!keyhook) {
    return 1;
  }

  if (UnhookWindowsHookEx(keyhook) == 0) {
    #ifdef DEBUG
    Error(L"UnhookWindowsHookEx(keyhook)", L"Could not unhook keyboard. Try restarting "APP_NAME".", GetLastError());
    #else
    if (showerror) {
      MessageBox(NULL, l10n.unhook_error, APP_NAME, MB_ICONINFORMATION|MB_OK|MB_TOPMOST|MB_SETFOREGROUND);
    }
    #endif
  }

  UnhookMouse();
  keyhook = NULL;
  UpdateTray();
  return 0;
}

void ToggleState() {
  if (ENABLED()) {
    UnhookKeyboard();
    KillTimer(g_hwnd, CHECKTIMER);
  }
  else {
    SendMessage(g_hwnd, WM_UPDATESETTINGS, 0, 0);
    HookKeyboard();
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_TRAY) {
    if (lParam == WM_LBUTTONDOWN || lParam == WM_LBUTTONDBLCLK || lParam == NIN_SELECT || lParam == NIN_KEYSELECT) {
      if (tray_left_click == TRAY_LEFT_TOGGLE) {
        ToggleState();
      }
      else {
        HookMouse();
      }
    }
    else if (lParam == WM_MBUTTONDOWN) {
      if ((GetAsyncKeyState(VK_SHIFT)&0x8000)) {
        ShellExecute(NULL, L"open", inipath, NULL, NULL, SW_SHOWNORMAL);
      }
      else {
        HookMouse();
      }
    }
    else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
      ShowContextMenu(hwnd);
    }
  }
  else if (msg == WM_UPDATESETTINGS) {
    LoadSettings();
  }
  else if (msg == WM_TASKBARCREATED) {
    tray_added = 0;
    UpdateTray();
  }
  else if (msg == WM_COMMAND) {
    int wmId = LOWORD(wParam);
    if (wmId == SWM_TOGGLE) {
      ToggleState();
    }
    else if (wmId == SWM_ELEVATE) {
      wchar_t path[MAX_PATH];
      GetModuleFileName(NULL, path, ARRAY_SIZE(path));
      int ret = (INT_PTR) ShellExecute(NULL, L"runas", path, NULL, NULL, SW_SHOWNORMAL);
      if (ret > 32) {
        DestroyWindow(hwnd);
      }
    }
    else if (wmId == SWM_AUTOSTART_ON) {
      SetAutostart(1, 0);
    }
    else if (wmId == SWM_AUTOSTART_OFF) {
      SetAutostart(0, 0);
    }
    else if (wmId == SWM_AUTOSTART_ELEVATE_ON) {
      SetAutostart(1, 1);
    }
    else if (wmId == SWM_AUTOSTART_ELEVATE_OFF) {
      SetAutostart(1, 0);
    }
    else if (wmId == SWM_ALWAYS_ELEVATE_ON) {
      WritePrivateProfileString(L"Advanced", L"AlwaysElevate", L"1", inipath);
      always_elevate = 1;
    }
    else if (wmId == SWM_ALWAYS_ELEVATE_OFF) {
      WritePrivateProfileString(L"Advanced", L"AlwaysElevate", L"0", inipath);
      always_elevate = 0;
    }
    else if (wmId == SWM_TIMERCHECK_ON) {
      WritePrivateProfileString(L"General", L"TimerCheck", L"1", inipath);
      SendMessage(g_hwnd, WM_UPDATESETTINGS, 0, 0);
    }
    else if (wmId == SWM_TIMERCHECK_OFF) {
      WritePrivateProfileString(L"General", L"TimerCheck", L"0", inipath);
      SendMessage(g_hwnd, WM_UPDATESETTINGS, 0, 0);
    }
    else if (wmId == SWM_SETTINGS) {
      ShellExecute(NULL, L"open", inipath, NULL, NULL, SW_SHOWNORMAL);
    }
    else if (wmId == SWM_WEBSITE) {
      OpenUrl(APP_URL);
    }
    else if (wmId == SWM_XKILL) {
      HookMouse();
    }
    else if (wmId == SWM_EXIT) {
      DestroyWindow(hwnd);
    }
  }
  else if (msg == WM_DESTROY) {
    showerror = 0;
    UnhookKeyboard();
    UnhookMouse();
    RestoreCursors();
    if (killcur) {
      DestroyCursor(killcur);
      killcur = NULL;
    }
    RemoveTray();
    ClearProcessDenylist();
    PostQuitMessage(0);
  }
  else if (msg == WM_TIMER) {
    if (wParam == CHECKTIMER && ENABLED()) {
      if (HotkeyHeld(&kill_hotkey)) {
        HWND foreground = GetForegroundWindow();
        if (foreground != NULL) {
          Kill(foreground);
        }
      }
      else {
        killing = 0;
      }
    }
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}
