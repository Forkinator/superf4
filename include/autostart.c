/*
  Copyright (C) 2019  Stefan Sundin
  Copyright (C) 2026  Forkinator fork contributors

  Autostart via HKCU Run, or Task Scheduler (ONLOGON + HIGHEST) when
  "Elevate on autostart" is enabled — avoids a UAC prompt every boot.
*/

#define AUTOSTART_TASK L"SuperF4"

static void ClearRunKey(void) {
  HKEY key;
  if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
    RegDeleteValue(key, APP_NAME);
    RegCloseKey(key);
  }
}

static int RunSchtasks(const wchar_t *args, int need_admin) {
  if (need_admin && !elevated) {
    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"schtasks.exe";
    sei.lpParameters = args;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
      return 0;
    }
    if (sei.hProcess) {
      WaitForSingleObject(sei.hProcess, 30000);
      DWORD code = 1;
      GetExitCodeProcess(sei.hProcess, &code);
      CloseHandle(sei.hProcess);
      return code == 0;
    }
    return 0;
  }

  wchar_t cmdline[1400];
  swprintf(cmdline, ARRAY_SIZE(cmdline), L"schtasks.exe %s", args);

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    return 0;
  }
  WaitForSingleObject(pi.hProcess, 20000);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return code == 0;
}

static int ElevatedTaskExists(void) {
  return RunSchtasks(L"/Query /TN \"" AUTOSTART_TASK L"\" /FO LIST", 0);
}

static void DeleteElevatedTask(void) {
  if (ElevatedTaskExists()) {
    RunSchtasks(L"/Delete /TN \"" AUTOSTART_TASK L"\" /F", 1);
  }
}

static int CreateElevatedTask(void) {
  wchar_t path[MAX_PATH];
  wchar_t args[1200];
  GetModuleFileName(NULL, path, ARRAY_SIZE(path));
  // One UAC prompt (if needed) to register a highest-privilege logon task
  swprintf(args, ARRAY_SIZE(args),
    L"/Create /TN \"" AUTOSTART_TASK L"\" /TR \"\\\"%s\\\"\" /SC ONLOGON /RL HIGHEST /F",
    path);
  return RunSchtasks(args, 1);
}

// Returns: 0=off, 1=on, 2=on+elevate (task or legacy Run -elevate)
int CheckAutostart() {
  if (ElevatedTaskExists()) {
    return 2;
  }

  HKEY key;
  wchar_t value[MAX_PATH+20] = L"";
  DWORD len = sizeof(value);

  if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return 0;
  }
  LONG err = RegQueryValueEx(key, APP_NAME, NULL, NULL, (LPBYTE)value, &len);
  RegCloseKey(key);
  if (err != ERROR_SUCCESS || value[0] == L'\0') {
    return 0;
  }

  wchar_t path[MAX_PATH], compare[MAX_PATH+20];
  GetModuleFileName(NULL, path, ARRAY_SIZE(path));
  swprintf(compare, ARRAY_SIZE(compare), L"\"%s\"", path);
  if (wcsstr(value, compare) != value) {
    return 0;
  }
  if (wcsstr(value, L" -elevate") != NULL) {
    return 2;
  }
  return 1;
}

void SetAutostart(int on, int elevate) {
  // Clear both mechanisms, then apply the requested mode
  ClearRunKey();
  DeleteElevatedTask();

  if (!on) {
    return;
  }

  if (elevate) {
    if (!CreateElevatedTask()) {
      Error(L"schtasks /Create", L"Could not create the elevated autostart task. You may have cancelled the UAC prompt.", GetLastError());
    }
    return;
  }

  HKEY key;
  int error = RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
  if (error != ERROR_SUCCESS) {
    Error(L"RegCreateKeyEx(HKEY_CURRENT_USER,'Software\\Microsoft\\Windows\\CurrentVersion\\Run')", L"Error opening the registry.", error);
    return;
  }

  wchar_t path[MAX_PATH], value[MAX_PATH+20];
  GetModuleFileName(NULL, path, ARRAY_SIZE(path));
  swprintf(value, ARRAY_SIZE(value), L"\"%s\"", path);
  error = RegSetValueEx(key, APP_NAME, 0, REG_SZ, (LPBYTE)value, (DWORD)((wcslen(value)+1)*sizeof(value[0])));
  if (error != ERROR_SUCCESS) {
    Error(L"RegSetValueEx('"APP_NAME"')", L"SetAutostart()", error);
  }
  RegCloseKey(key);
}
