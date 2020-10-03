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

#pragma once

#include "ConsoleDialog.h"
#include "PluginInterface.h"

#include "serial/serial.h"

#define RUN_LUA 0xA7
#define RUN_LUA_OK 0x31
#define RUN_LUA_ERROR 0x30

class LuaConsole final {
public:
	explicit LuaConsole(HWND hNotepad);

	~LuaConsole() {
		delete console;
		delete npp_data;
		hwnd_notepad = nullptr;
	}

	void init(HINSTANCE hInst, NppData& nppData) {
		console->initDialog(hInst, nppData, this);
		*npp_data = nppData;
	}

	void setComPort(std::string& port) { this->port = port; }

	bool runStatement(const char* statement);

	void setupInput(GUI::ScintillaWindow &sci);
	void setupOutput(GUI::ScintillaWindow &sci);

	bool processNotification(const SCNotification *scn);

	void showAutoCompletion();

	ConsoleDialog *console;
private:
	NppData* npp_data;
	HWND hwnd_notepad;

	// Autocomplete lists
	std::vector<std::string> ez_props;
	std::vector<std::string> ez_funcs;

	std::string port;

	GUI::ScintillaWindow *sci_input;

	void maintainIndentation();
	void braceMatch();
};

