// This file is part of ezLCDLua
// 
// Copyright (C)2016 Justin Dailey <dail8859@yahoo.com>
// Copyright (C)2020 Earth Computer Technologies, Inc
// 
// ezLCDLua is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <Windows.h>
#include <Shellapi.h>
#include <shlwapi.h>
#include <vector>

#include "PluginInterface.h"
#include "AboutDialog.h"
#include "LuaConsole.h"
#include "SciLexer.h"


// --- Menu callbacks ---
static void showConsole();
static void editSettings();
static void executeCurrentFile();
static void showAbout();

// --- Local variables ---
static NppData nppData;
static HWND curScintilla;
static HINSTANCE hInstance;
static LuaConsole *luaConsole;
static bool isReady = false;
static std::vector<FuncItem> funcItems;
static ShortcutKey shortcut;
static toolbarIcons tbiConsoleButton;


static LRESULT SendNpp(UINT Msg, WPARAM wParam = SCI_UNUSED, LPARAM lParam = SCI_UNUSED) {
	return SendMessage(nppData._nppHandle, Msg, wParam, lParam);
}

static HWND updateScintilla() {
	// Get the current scintilla
	int which = -1;
	SendNpp(NPPM_GETCURRENTSCINTILLA, SCI_UNUSED, (LPARAM)&which);
	return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

static const wchar_t* GetIniFilePath() {
	static wchar_t iniPath[MAX_PATH];
	SendNpp(NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)iniPath);
	wcscat_s(iniPath, MAX_PATH, L"\\ezLCDLua.ini");
	return iniPath;
}

static void ReadSettings() {
	wchar_t com_port[1024] = { 0 };
	GetPrivateProfileString(TEXT("ezLCDLua"), TEXT("PORT"), TEXT(""), com_port, 1023, GetIniFilePath());
	luaConsole->setComPort(GUI::UTF8FromString(com_port));
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  reasonForCall, LPVOID lpReserved) {
	switch (reasonForCall) {
		case DLL_PROCESS_ATTACH:
			hInstance = (HINSTANCE)hModule;
			break;
		case DLL_PROCESS_DETACH:
			if (tbiConsoleButton.hToolbarBmp) ::DeleteObject(tbiConsoleButton.hToolbarBmp);
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
	}

	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notepadPlusData) {
	nppData = notepadPlusData;

	// Initialize the dialog
	luaConsole = new LuaConsole(nppData, hInstance);
}

extern "C" __declspec(dllexport) const wchar_t *getName() {
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *nbF) {
	shortcut._isAlt = FALSE;
	shortcut._isShift = FALSE;
	shortcut._isCtrl = TRUE;
	shortcut._key = 'E';

	// Set up the shortcuts
	funcItems.emplace_back(FuncItem{ TEXT("Show Console"), showConsole, 0, false, NULL });
	funcItems.emplace_back(FuncItem{ TEXT("Execute Current File"), executeCurrentFile, 0, false, &shortcut });
	funcItems.emplace_back(FuncItem{ TEXT(""), nullptr, 0, false, nullptr }); // separator
	funcItems.emplace_back(FuncItem{ TEXT("Edit Config File"), editSettings, 0, false, NULL });
	funcItems.emplace_back(FuncItem{ TEXT("About..."), showAbout, 0, false, NULL });

	*nbF = static_cast<int>(funcItems.size());
	return funcItems.data();
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notification) {
	Sci_NotifyHeader nh = notification->nmhdr;

	switch (nh.code) {
		case NPPN_READY: {
			const char *msg = "ezLCD Lua Plugin\r\n\r\n";
			luaConsole->console->writeText(strlen(msg), msg);
			luaConsole->console->setCommandID(&funcItems[0]._cmdID);
			isReady = true;

			// If ini doesnt exist create it with default values
			if (PathFileExists(GetIniFilePath()) == 0) {
				WritePrivateProfileString(TEXT("ezLCDLua"), TEXT("PORT"), TEXT("COM1"), GetIniFilePath());
			}

			ReadSettings();

			break;
		}
		case NPPN_TBMODIFICATION: {
			tbiConsoleButton.hToolbarIcon = NULL;
			tbiConsoleButton.hToolbarBmp = (HBITMAP)::LoadImage(hInstance, MAKEINTRESOURCE(IDI_LUABMP), IMAGE_BITMAP, 0, 0, (LR_SHARED | LR_LOADTRANSPARENT | LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
			SendNpp(NPPM_ADDTOOLBARICON, (WPARAM)funcItems[0]._cmdID, (LPARAM)&tbiConsoleButton);
			break;
		}
		case NPPN_FILESAVED: {
			wchar_t fname[MAX_PATH] = { 0 };
			SendMessage(nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, notification->nmhdr.idFrom, (LPARAM)fname);

			if (wcscmp(fname, GetIniFilePath()) == 0) {
				ReadSettings();
			}
			break;
		}
		case NPPN_SHUTDOWN:
			break;
	}
	return;
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT message, WPARAM wParam, LPARAM lParam) {
	return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() {
	return TRUE;
}

// --- Menu call backs ---

void showConsole() {
	if (luaConsole->console->isVisible())
		luaConsole->console->hide();
	else
		luaConsole->console->doDialog();

	// If the console is shown automatically at startup, don't give it focus
	if (isReady) luaConsole->console->giveInputFocus();
}

static void editSettings() {
	SendNpp(NPPM_DOOPEN, 0, (LPARAM)GetIniFilePath());
}

static void executeCurrentFile() {
	HWND current_scintilla = updateScintilla();
	const char* doc = (const char*)SendMessage(current_scintilla, SCI_GETCHARACTERPOINTER, SCI_UNUSED, SCI_UNUSED);

	if (luaConsole->runStatement(doc) == false) {
		luaConsole->console->doDialog();
		SendMessage(current_scintilla, SCI_GRABFOCUS, SCI_UNUSED, SCI_UNUSED);
	}
}

static void showAbout() {
	ShowAboutDialog(hInstance, MAKEINTRESOURCE(IDD_ABOUTDLG), nppData._nppHandle);
}
