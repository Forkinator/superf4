/*
  Copyright (C) 2019  Stefan Sundin
  Copyright (C) 2026  Forkinator fork contributors

  Hotkey parsing/matching for SuperF4.ini KillHotkey / XkillHotkey.
*/

struct Hotkey {
  int ctrl;
  int alt;
  int shift;
  int win;
  int vk;
};

static int ModDown(int left, int right) {
  return ((GetAsyncKeyState(left) & 0x8000) || (GetAsyncKeyState(right) & 0x8000)) ? 1 : 0;
}

static int ParseVkToken(const wchar_t *tok) {
  if (!_wcsicmp(tok, L"F1")) return VK_F1;
  if (!_wcsicmp(tok, L"F2")) return VK_F2;
  if (!_wcsicmp(tok, L"F3")) return VK_F3;
  if (!_wcsicmp(tok, L"F4")) return VK_F4;
  if (!_wcsicmp(tok, L"F5")) return VK_F5;
  if (!_wcsicmp(tok, L"F6")) return VK_F6;
  if (!_wcsicmp(tok, L"F7")) return VK_F7;
  if (!_wcsicmp(tok, L"F8")) return VK_F8;
  if (!_wcsicmp(tok, L"F9")) return VK_F9;
  if (!_wcsicmp(tok, L"F10")) return VK_F10;
  if (!_wcsicmp(tok, L"F11")) return VK_F11;
  if (!_wcsicmp(tok, L"F12")) return VK_F12;
  if (!_wcsicmp(tok, L"ESC") || !_wcsicmp(tok, L"ESCAPE")) return VK_ESCAPE;
  if (!_wcsicmp(tok, L"SPACE")) return VK_SPACE;
  if (!_wcsicmp(tok, L"TAB")) return VK_TAB;
  if (!_wcsicmp(tok, L"PAUSE")) return VK_PAUSE;
  if (!_wcsicmp(tok, L"DELETE") || !_wcsicmp(tok, L"DEL")) return VK_DELETE;
  if (!_wcsicmp(tok, L"INSERT") || !_wcsicmp(tok, L"INS")) return VK_INSERT;
  if (!_wcsicmp(tok, L"HOME")) return VK_HOME;
  if (!_wcsicmp(tok, L"END")) return VK_END;
  if (!_wcsicmp(tok, L"PRIOR") || !_wcsicmp(tok, L"PGUP")) return VK_PRIOR;
  if (!_wcsicmp(tok, L"NEXT") || !_wcsicmp(tok, L"PGDN")) return VK_NEXT;
  if (wcslen(tok) == 1) {
    wchar_t c = tok[0];
    if (c >= L'a' && c <= L'z') c = (wchar_t)(c - 32);
    if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
      return (int)c;
    }
  }
  return 0;
}

static wchar_t *NextHotkeyToken(wchar_t **cursor) {
  wchar_t *start = *cursor;
  if (start == NULL || *start == L'\0') {
    return NULL;
  }
  while (*start == L' ' || *start == L'\t') {
    start++;
  }
  if (*start == L'\0') {
    *cursor = start;
    return NULL;
  }
  wchar_t *p = start;
  while (*p && *p != L'+') {
    p++;
  }
  if (*p == L'+') {
    *p = L'\0';
    *cursor = p + 1;
  }
  else {
    *cursor = p;
  }
  wchar_t *end = p;
  while (end > start && (end[-1] == L' ' || end[-1] == L'\t')) {
    *--end = L'\0';
  }
  return start;
}

int ParseHotkey(const wchar_t *text, struct Hotkey *out, const struct Hotkey *fallback) {
  struct Hotkey hk = {0, 0, 0, 0, 0};
  if (text == NULL || text[0] == L'\0') {
    *out = *fallback;
    return 0;
  }

  wchar_t buf[128];
  wcsncpy(buf, text, ARRAY_SIZE(buf) - 1);
  buf[ARRAY_SIZE(buf) - 1] = L'\0';

  wchar_t *cursor = buf;
  wchar_t *tok;
  while ((tok = NextHotkeyToken(&cursor)) != NULL) {
    if (!_wcsicmp(tok, L"Ctrl") || !_wcsicmp(tok, L"Control")) {
      hk.ctrl = 1;
    }
    else if (!_wcsicmp(tok, L"Alt")) {
      hk.alt = 1;
    }
    else if (!_wcsicmp(tok, L"Shift")) {
      hk.shift = 1;
    }
    else if (!_wcsicmp(tok, L"Win") || !_wcsicmp(tok, L"Windows") || !_wcsicmp(tok, L"Super")) {
      hk.win = 1;
    }
    else {
      int vk = ParseVkToken(tok);
      if (vk == 0 || hk.vk != 0) {
        *out = *fallback;
        return 0;
      }
      hk.vk = vk;
    }
  }

  if (hk.vk == 0) {
    *out = *fallback;
    return 0;
  }
  *out = hk;
  return 1;
}

int HotkeyMatch(const struct Hotkey *hk, int vkey) {
  if (hk == NULL || vkey != hk->vk) {
    return 0;
  }
  if (hk->ctrl != ModDown(VK_LCONTROL, VK_RCONTROL)) return 0;
  if (hk->alt != ModDown(VK_LMENU, VK_RMENU)) return 0;
  if (hk->shift != ModDown(VK_LSHIFT, VK_RSHIFT)) return 0;
  if (hk->win != ModDown(VK_LWIN, VK_RWIN)) return 0;
  return 1;
}

int HotkeyHeld(const struct Hotkey *hk) {
  if (hk == NULL || hk->vk == 0) {
    return 0;
  }
  if (!(GetAsyncKeyState(hk->vk) & 0x8000)) {
    return 0;
  }
  return HotkeyMatch(hk, hk->vk);
}