/*
  Copyright (C) 2019  Stefan Sundin

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

NOTIFYICONDATA tray;
HICON icon[2];
int tray_added = 0;

int InitTray() {
  // Load icons
  icon[0] = LoadImage(g_hinst, L"tray_disabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  icon[1] = LoadImage(g_hinst, L"tray_enabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  if (icon[0] == NULL || icon[1] == NULL) {
    Error(L"LoadImage('tray_*')", L"Could not load tray icons.", GetLastError());
    return 1;
  }

  // Create icondata
  ZeroMemory(&tray, sizeof(tray));
  tray.cbSize = sizeof(NOTIFYICONDATA);
  tray.uID = 0;
  tray.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;
  tray.hWnd = g_hwnd;
  tray.uCallbackMessage = WM_TRAY;

  // Register TaskbarCreated so we can re-add the tray icon if (when) explorer.exe crashes
  WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");

  return 0;
}

int UpdateTray() {
  wcsncpy(tray.szTip, (ENABLED() && elevated?l10n.tray_elevated:ENABLED()?l10n.tray_enabled:l10n.tray_disabled), ARRAY_SIZE(tray.szTip)-1);
  tray.szTip[ARRAY_SIZE(tray.szTip)-1] = L'\0';
  tray.hIcon = icon[ENABLED()?1:0];
  tray.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;

  // Try once. If it doesn't succeed, try again later — never block (keyboard hook latency).
  if (Shell_NotifyIcon((tray_added?NIM_MODIFY:NIM_ADD), &tray)) {
    if (!tray_added) {
      tray.uVersion = NOTIFYICON_VERSION_4;
      Shell_NotifyIcon(NIM_SETVERSION, &tray);
    }
    tray_added = 1;
  }
  return 0;
}

int RemoveTray() {
  if (!tray_added) {
    // Tray not added
    return 1;
  }

  if (Shell_NotifyIcon(NIM_DELETE,&tray) == FALSE) {
    Error(L"Shell_NotifyIcon(NIM_DELETE)", L"Failed to remove tray icon.", GetLastError());
    return 1;
  }

  // Success
  tray_added = 0;
  return 0;
}

void ShowKillMessage(const wchar_t *process_name) {
  wchar_t info[256];
  swprintf(info, ARRAY_SIZE(info), L"Killed: %s", process_name);

  tray.uFlags = NIF_INFO|NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;
  wcsncpy(tray.szInfoTitle, APP_NAME, ARRAY_SIZE(tray.szInfoTitle)-1);
  tray.szInfoTitle[ARRAY_SIZE(tray.szInfoTitle)-1] = L'\0';
  wcsncpy(tray.szInfo, info, ARRAY_SIZE(tray.szInfo)-1);
  tray.szInfo[ARRAY_SIZE(tray.szInfo)-1] = L'\0';
  tray.dwInfoFlags = NIIF_INFO;
  tray.uTimeout = 3000;
  Shell_NotifyIcon(NIM_MODIFY, &tray);

  // Clear info flags so later UpdateTray calls do not re-show the balloon
  tray.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;
  tray.szInfo[0] = L'\0';
  tray.szInfoTitle[0] = L'\0';
}

void ShowContextMenu(HWND hwnd) {
  POINT pt;
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  if (menu == NULL) {
    return;
  }

  // Check autostart
  int autostart = CheckAutostart();
  // Check TimerCheck
  wchar_t txt[10];
  GetPrivateProfileString(L"General", L"TimerCheck", L"0", txt, ARRAY_SIZE(txt), inipath);
  short timercheck = _wtoi(txt);

  // Menu
  InsertMenu(menu, -1, MF_BYPOSITION, SWM_TOGGLE, (ENABLED()?l10n.menu.disable:l10n.menu.enable));
  InsertMenu(menu, -1, MF_BYPOSITION|(elevated?MF_DISABLED|MF_GRAYED:0), SWM_ELEVATE, (elevated?l10n.menu.elevated:l10n.menu.elevate));

  // Options
  HMENU menu_options = CreatePopupMenu();
  if (menu_options) {
    InsertMenu(menu_options, -1, MF_BYPOSITION|(autostart?MF_CHECKED:0), (autostart?SWM_AUTOSTART_OFF:SWM_AUTOSTART_ON), l10n.menu.autostart);
    InsertMenu(menu_options, -1, MF_BYPOSITION|(autostart==2?MF_CHECKED:0), (autostart==2?SWM_AUTOSTART_ELEVATE_OFF:SWM_AUTOSTART_ELEVATE_ON), l10n.menu.autostart_elevate);
    InsertMenu(menu_options, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
    InsertMenu(menu_options, -1, MF_BYPOSITION|(timercheck?MF_CHECKED:0), (timercheck?SWM_TIMERCHECK_OFF:SWM_TIMERCHECK_ON), l10n.menu.timercheck);
    InsertMenu(menu_options, -1, MF_BYPOSITION, SWM_SETTINGS, l10n.menu.open_ini);
    InsertMenu(menu_options, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
    InsertMenu(menu_options, -1, MF_BYPOSITION, SWM_WEBSITE, l10n.menu.website);
    InsertMenu(menu_options, -1, MF_BYPOSITION|MF_DISABLED|MF_GRAYED, 0, l10n.menu.version);
    InsertMenu(menu, -1, MF_BYPOSITION|MF_POPUP, (UINT_PTR)menu_options, l10n.menu.options);
  }

  InsertMenu(menu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
  InsertMenu(menu, -1, MF_BYPOSITION, SWM_XKILL, l10n.menu.xkill);

  InsertMenu(menu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
  InsertMenu(menu, -1, MF_BYPOSITION, SWM_EXIT, l10n.menu.exit);

  // Track menu
  SetForegroundWindow(hwnd);
  TrackPopupMenu(menu, TPM_BOTTOMALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
  // Required so the menu dismisses correctly on click-away
  PostMessage(hwnd, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

int OpenUrl(wchar_t *url) {
  int ret = (INT_PTR) ShellExecute(NULL, L"open", url, NULL, NULL, SW_SHOWDEFAULT);
  if (ret <= 32 && MessageBox(NULL,L"Failed to open browser. Copy url to clipboard?",APP_NAME,MB_ICONWARNING|MB_YESNO) == IDYES) {
    int size = (wcslen(url)+1)*sizeof(url[0]);
    wchar_t *data = LocalAlloc(LMEM_FIXED, size);
    if (data == NULL) {
      return ret;
    }
    memcpy(data, url, size);
    if (OpenClipboard(NULL)) {
      EmptyClipboard();
      SetClipboardData(CF_UNICODETEXT, data);
      CloseClipboard();
    }
    else {
      LocalFree(data);
    }
  }
  return ret;
}
