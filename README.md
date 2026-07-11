# SuperF4 (Forkinator fork)

Fork of [stefansundin/superf4](https://github.com/stefansundin/superf4) with bug fixes and Windows 11 updates. Upstream project site: [stefansundin.github.io/superf4](https://stefansundin.github.io/superf4/).

## Changes in this fork (v1.4.2)

### Standard (on by default)
- **Left-click tray = xkill** (`TrayLeftClick=xkill`)
- **Safer builtin denylist** — always blocks critical processes (`dwm`, `csrss`, `winlogon`, `svchost`, `taskmgr`, …) even if you clear the ini list
- **Elevate on autostart via Task Scheduler** — one UAC when enabling; no prompt every boot
- **Always elevate** toggle in the Options menu
- **One-time first-run tip** explaining tray clicks
- High-DPI xkill hit-testing, no fullscreen overlay (avoids Win11 Do Not Disturb), message-only window, single-instance, Win11 tray APIs

### Optional (ini / menu)
- `TrayLeftClick=toggle` — classic left-click enable/disable
- `KillHotkey` / `XkillHotkey` — e.g. `Ctrl+Alt+F4`, `Win+F4`, `Ctrl+Shift+Q`
- `ShowKillMessage=1` — balloon after a kill
- `TimerCheck=1` — poll hotkey when games block hooks
- `AlwaysElevate=1` — or Options → Always elevate
- `ShowFirstRunTip=1` — show the tip again
- **Authenticode signing** — set `SUPERF4_PFX` + `SUPERF4_PFX_PASSWORD` when running `build.bat release`

### SmartScreen / unsigned builds
Windows may warn on download because builds are not Microsoft-attested. Prefer the installer or zip from [Releases](https://github.com/Forkinator/superf4/releases). If SmartScreen blocks the exe: Properties → **Unblock**, or use “More info” → Run anyway. Signing with your own code-signing certificate (see `build.bat`) removes most warnings.

### Downloads
Release assets match Stefan’s layout: `SuperF4-x.y.z.exe` (auto 32/64 installer), plus `*-32bit.zip` / `*-64bit.zip` portables.

---

# SuperF4

### Note: SuperF4 is a Windows only application!

## Donate

SuperF4 is free software and a one-man endeavor. If you find it useful, please make a donation. I greatly appreciate any support!

You can also support Stefan through [GitHub Sponsorship](https://github.com/sponsors/stefansundin) (there are perks), or through [Patreon](https://www.patreon.com/stefansundin).

Contact Stefan at stefaNStefansundinCom if donation options do not work.

## What is SuperF4?

SuperF4 kills the foreground program when you press **Ctrl+Alt+F4**. This is different from when you press Alt+F4. When you press Alt+F4, the program can refuse to quit. Windows only asks the program to quit, and lets it decide for itself what to do.

You can also kill a program by pressing **Win+F4** and then clicking the window with your mouse cursor. You can press Escape or the right mouse button to exit this mode without killing a program.

Some games have anti-keylogger protection, which may prevent SuperF4 from working (it can't detect when you press Ctrl+Alt+F4). You can enable **TimerCheck** to use an alternate detection method.

## News

**2019-02-16 - SuperF4 v1.4 released**

Released 1.4.

- Fixed keyboard input lag. Thanks to Victor Robertson.
- Do not enter xkill mode if Ctrl key is pressed. Ctrl+Win+F4 is a new shortcut to close virtual desktops, so we don’t want to prevent that. Thanks to José Rebelo.
- Add a process blacklist, with explorer.exe in the list by default.

**2015-08-23 - SuperF4 v1.3 released**

Released 1.3.

The installer now defaults to install to the `%APPDATA%` directory, so it won’t ask for administrator privileges when you start it. If you want to install to Program Files, right-click the installer and select Run as administrator.

Due to this change in the installer, it is recommended that you uninstall and install from scratch if you are upgrading from a previous version.

For an RSS feed with only new releases, use the [GitHub release feed](https://github.com/stefansundin/superf4/releases.atom).

## Support

Before reporting a bug, look through the list of [issues on GitHub](https://github.com/stefansundin/superf4/issues).

When reporting a bug, please specify what version of Windows you are using, and be sure to include any error message you might see.

For issues specific to this fork, open an issue on [Forkinator/superf4](https://github.com/Forkinator/superf4/issues).

## License

SuperF4 is free software and licensed under [GNU GPL v3](LICENSE). Get the upstream source code on [GitHub](https://github.com/stefansundin/superf4).

You may redistribute SuperF4 on your own website, but please provide a link to the project website on the same page. Please do not bundle SuperF4 together with toolbars or other annoying adware, or otherwise degrade the experience.

## Credits

Made by [Stefan Sundin](https://stefansundin.com/).

You can browse his [other projects](https://github.com/stefansundin), or visit his [personal website](https://stefansundin.com/).

This fork is maintained at [Forkinator/superf4](https://github.com/Forkinator/superf4).
