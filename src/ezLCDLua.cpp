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
static SciFnDirect pSciMsg;			// For direct scintilla call
static sptr_t pSciWndData;			// For direct scintilla call
static HANDLE _hModule;				// For dialog initialization
static LuaConsole *g_console;
static bool isReady = false;
static std::vector<FuncItem> funcItems;
static toolbarIcons tbiConsoleButton;

static inline LRESULT SendScintilla(UINT Msg, WPARAM wParam = SCI_UNUSED, LPARAM lParam = SCI_UNUSED) {
	return pSciMsg(pSciWndData, Msg, wParam, lParam);
}

static inline LRESULT SendNpp(UINT Msg, WPARAM wParam = SCI_UNUSED, LPARAM lParam = SCI_UNUSED) {
	return SendMessage(nppData._nppHandle, Msg, wParam, lParam);
}

static bool updateScintilla() {
	// Get the current scintilla
	int which = -1;
	SendNpp(NPPM_GETCURRENTSCINTILLA, SCI_UNUSED, (LPARAM)&which);
	if(which == -1) return false;
	curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

	// Get the function and pointer to it for more efficient calls
	pSciMsg = (SciFnDirect) SendMessage(curScintilla, SCI_GETDIRECTFUNCTION, 0, 0);
	pSciWndData = (sptr_t) SendMessage(curScintilla, SCI_GETDIRECTPOINTER, 0, 0);

	return true;
}

const wchar_t* GetIniFilePath() {
	static wchar_t iniPath[MAX_PATH];
	SendNpp(NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)iniPath);
	wcscat_s(iniPath, MAX_PATH, L"\\ezLCDLua.ini");
	return iniPath;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  reasonForCall, LPVOID lpReserved) {
	switch (reasonForCall) {
		case DLL_PROCESS_ATTACH:
			_hModule = hModule;
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

	// Initialize the dialogs
	g_console = new LuaConsole(nppData._nppHandle);
	g_console->init((HINSTANCE)_hModule, nppData);

	// Set up the short cuts that we know of
	funcItems.emplace_back(FuncItem{ TEXT("Show Console"), showConsole, 0, false, NULL });
	funcItems.emplace_back(FuncItem{ TEXT("Execute Current File"), executeCurrentFile, 0, false, NULL });
	funcItems.emplace_back(FuncItem{ TEXT(""), nullptr, 0, false, nullptr }); // separator
	funcItems.emplace_back(FuncItem{ TEXT("Edit Config File"), editSettings, 0, false, NULL });
	funcItems.emplace_back(FuncItem{ TEXT("About..."), showAbout, 0, false, NULL });
}

extern "C" __declspec(dllexport) const wchar_t *getName() {
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *nbF) {
	*nbF = static_cast<int>(funcItems.size());
	return funcItems.data();
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notification) {
	Sci_NotifyHeader nh = notification->nmhdr;

	switch (nh.code) {
		case NPPN_READY: {
			const char *msg = "ezLCD Lua Plugin\r\n\r\n";
			g_console->console->writeText(strlen(msg), msg);
			g_console->console->setCommandID(&funcItems[0]._cmdID);
			isReady = true;

			// If ini doesnt exist create it with default values
			if (PathFileExists(GetIniFilePath()) == 0) {
				WritePrivateProfileString(TEXT("ezLCDLua"), TEXT("PORT"), TEXT("COM1"), GetIniFilePath());
			}

			wchar_t test[1024] = { 0 };
			GetPrivateProfileString(TEXT("ezLCDLua"), TEXT("PORT"), TEXT(""), test, 1023, GetIniFilePath());
			g_console->setComPort(GUI::UTF8FromString(test));
			break;
		}
		case NPPN_TBMODIFICATION:
			tbiConsoleButton.hToolbarIcon = NULL;
			tbiConsoleButton.hToolbarBmp = (HBITMAP)::LoadImage((HINSTANCE)_hModule, MAKEINTRESOURCE(IDI_LUABMP), IMAGE_BITMAP, 0, 0, (LR_SHARED | LR_LOADTRANSPARENT | LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
			SendNpp(NPPM_ADDTOOLBARICON, (WPARAM)funcItems[0]._cmdID, (LPARAM)&tbiConsoleButton);
			break;
		case NPPN_FILEOPENED:
			//SendNpp(NPPM_GETFULLPATHFROMBUFFERID, nh.idFrom, (LPARAM)fname);
			//LuaExtension::Instance().OnOpen(GUI::UTF8FromString(fname).c_str(), nh.idFrom);
			break;
		case NPPN_FILESAVED: {
			wchar_t fname[MAX_PATH] = { 0 };
			SendMessage(nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, notification->nmhdr.idFrom, (LPARAM)fname);
			if (wcscmp(fname, GetIniFilePath()) == 0) {
				wchar_t test[1024] = { 0 };
				GetPrivateProfileString(TEXT("ezLCDLua"), TEXT("PORT"), TEXT(""), test, 1023, GetIniFilePath());
				g_console->setComPort(GUI::UTF8FromString(test));
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
	if (g_console->console->isVisible())
		g_console->console->hide();
	else
		g_console->console->doDialog();

	// If the console is shown automatically at startup, don't give it focus
	if (isReady) g_console->console->giveInputFocus();
}

static void editSettings() {
	SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)GetIniFilePath());
}

static void executeCurrentFile() {
	updateScintilla();
	const char *doc = (const char *)SendScintilla(SCI_GETCHARACTERPOINTER);

	if (g_console->runStatement(doc) == false) {
		g_console->console->doDialog();
	}
}

static void showAbout() {
	ShowAboutDialog((HINSTANCE)_hModule, MAKEINTRESOURCE(IDD_ABOUTDLG), nppData._nppHandle);
}
