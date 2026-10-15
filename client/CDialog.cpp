/*
===============================================================================

    Battle City - CDialog
    Copyright (C) 2005-2013  battlecity.org

    This file is part of Battle City.

    Battle City is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Battle City is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Battle City.  If not, see <http://www.gnu.org/licenses/>.
===============================================================================
*/
#include "CDialog.h"


CDialog::CDialog(CGame *game)
{
	p = game;
	this->StartDialog = 0;
	this->DialogProcessor = 0;
}

CDialog::~CDialog()
{

}
/// <summary>   Starts the Dialog by creating a thread for thrDialog with zero stack size, and CGame as argument </summary>
void CDialog::Start()
{
	// Dialogs used to be driven by a thread; they are now opened from Cycle() on the main thread
}

void CDialog::Cycle()
{
	if (this->StartDialog > 0 && this->DialogProcessor != 0 && !BCDialogsActive())
	{
		DialogBox(p->hInst, MAKEINTRESOURCE(this->StartDialog), p->hWnd, this->DialogProcessor);
	}
}
