/*
  Copyright (C) 2019  Stefan Sundin
  Copyright (C) 2026  Forkinator fork contributors

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

NOTIFYICONDATA tray;
HICON icon[2];
int tray_added = 0;

int InitTray() {
  icon[0] = LoadImage(g_hinst, L"tray_disabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  icon[1] = LoadImage(g_hinst, L"tray_enabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  if (icon[0] == NULL || icon[1] == NULL) {
    Error(L"LoadImage('tray_*')", L"Could not load tray icons.", GetLastError());
    return 1;
  }

  ZeroMemory(&tray, sizeof(tray));
  tray.cbSize = sizeof(NOTIFYICONDATA);
  tray.uID = 0;
  tray.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;
  tray.hWnd = g_hwnd;
  tray.uCallbackMessage = WM_TRAY;

  WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");
  return 0;
}

int UpdateTray() {
  wcsncpy(tray.szTip, (ENABLED() && elevated?l10n.tray_elevated:ENABLED()?l10n.tray_enabled:l10n.tray_disabled), ARRAY_SIZE(tray.szTip)-1);
  tray.szTip[ARRAY_SIZE(tray.szTip)-1] = L'\0';
  tray.hIcon = icon[ENABLED()?1:0];
  tray.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;

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
    return 1;
  }

  if (Shell_NotifyIcon(NIM_DELETE,&tray) == FALSE) {
    Error(L"Shell_NotifyIcon(NIM_DELETE)", L"Failed to remove tray icon.", GetLastError());
    return 1;
  }

  tray_added = 0;
  return 0;
}

void ShowTrayBalloon(const wchar_t *title, const wchar_t *text) {
  if (!tray_added) {
    UpdateTray();
  }

  tray.uFlags = NIF_INFO|NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;
  wcsncpy(tray.szInfoTitle, title, ARRAY_SIZE(tray.szInfoTitle)-1);
  tray.szInfoTitle[ARRAY_SIZE(tray.szInfoTitle)-1] = L'\0';
  wcsncpy(tray.szInfo, text, ARRAY_SIZE(tray.szInfo)-1);
  tray.szInfo[ARRAY_SIZE(tray.szInfo)-1] = L'\0';
  tray.dwInfoFlags = NIIF_INFO;
  tray.uTimeout = 8000;
  Shell_NotifyIcon(NIM_MODIFY, &tray);

  tray.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;
  tray.szInfo[0] = L'\0';
  tray.szInfoTitle[0] = L'\0';
}

void ShowKillMessage(const wchar_t *process_name) {
  wchar_t info[256];
  swprintf(info, ARRAY_SIZE(info), L"Killed: %s", process_name);
  ShowTrayBalloon(APP_NAME, info);
}

void MaybeShowFirstRunTip() {
  if (!show_first_run_tip) {
    return;
  }
  ShowTrayBalloon(
    APP_NAME,
    L"Left-click: xkill. Right-click: menu (enable/disable, options). Edit the ini for hotkeys and TrayLeftClick=toggle."
  );
  WritePrivateProfileString(L"General", L"ShowFirstRunTip", L"0", inipath);
  show_first_run_tip = 0;
}

void ShowContextMenu(HWND hwnd) {
  POINT pt;
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  if (menu == NULL) {
    return;
  }

  int autostart = CheckAutostart();
  wchar_t txt[10];
  GetPrivateProfileString(L"General", L"TimerCheck", L"0", txt, ARRAY_SIZE(txt), inipath);
  short timercheck = _wtoi(txt);

  InsertMenu(menu, -1, MF_BYPOSITION, SWM_TOGGLE, (ENABLED()?l10n.menu.disable:l10n.menu.enable));
  InsertMenu(menu, -1, MF_BYPOSITION|(elevated?MF_DISABLED|MF_GRAYED:0), SWM_ELEVATE, (elevated?l10n.menu.elevated:l10n.menu.elevate));

  HMENU menu_options = CreatePopupMenu();
  if (menu_options) {
    InsertMenu(menu_options, -1, MF_BYPOSITION|(autostart?MF_CHECKED:0), (autostart?SWM_AUTOSTART_OFF:SWM_AUTOSTART_ON), l10n.menu.autostart);
    InsertMenu(menu_options, -1, MF_BYPOSITION|(autostart==2?MF_CHECKED:0), (autostart==2?SWM_AUTOSTART_ELEVATE_OFF:SWM_AUTOSTART_ELEVATE_ON), l10n.menu.autostart_elevate);
    InsertMenu(menu_options, -1, MF_BYPOSITION|(always_elevate?MF_CHECKED:0), (always_elevate?SWM_ALWAYS_ELEVATE_OFF:SWM_ALWAYS_ELEVATE_ON), l10n.menu.always_elevate);
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

  SetForegroundWindow(hwnd);
  TrackPopupMenu(menu, TPM_BOTTOMALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
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
