/*
===============================================================================

    Battle City - CEngine
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
#include "CEngine.h"
#include <map>
/// <summary>   Game engine: one Frame() per main-loop iteration </summary>
///
/// <param name="pParam">   [in,out] If non-null, the parameter. </param>
/// <summary>   Constructor. Sets default values to variables </summary>
///
/// <param name="game"> [in,out] If non-null, the game. </param>
CEngine::CEngine(CGame *game) {
	p = game;
	this->FPS = 0;
	this->FPSTick = 0;
	this->loops = 0;
	this->MsgUp = 0;
	this->MsgQueue = 0;
	this->NextFrame = 0;
	this->lastFPS = 0;
	this->lastlastFPS = 0;
}
/// <summary>   Destructor. </summary>
CEngine::~CEngine() {

}
/// <summary>   Prepares the engine </summary>
void CEngine::Init() {
	// The engine used to run on its own thread; it now runs from the main loop via Frame()
	this->NextFrame = 0;
}
/// <summary>   One iteration of the game loop (called from the main loop) </summary>
void CEngine::Frame() {
	char FPSString[20];
	bool slowWarningDue = false;
	if (p->running != 1) return;

	// Open any dialog that the game state asked for
	p->Dialog->Cycle();

	// Not time for the next frame yet: keep the network serviced and come back
	if (p->Timer->GetTime() < NextFrame) {
		p->Winsock->Cycle();
		return;
	}

        //  Update last time interval with current time interval
		p->lastTick = p->Tick;
        //  Set current time interval with new time interval
		p->Tick = p->Timer->GetTime();
        //  Set time passed by calling the difference between current time interval and previous time interval
		p->TimePassed = p->Tick - p->lastTick;
        //  Observe the current game state
		switch (p->State) {
            //  Perform game cycle and draw debug information
			case STATE_GAME:
                //  Clear DirectDraw canvas
				p->DDraw->Clear();
                //  Perform game cycle
				p->InGame->Cycle();
                //  Upon verifying debug mode is set FPSString with debug information
				if (p->Options->debug) {
                    //  Compile string and assign to FPSString
					sprintf(FPSString, "FPS: %i", p->Engine->FPS);
                    //  Draw text at bottom right using white font color
					p->DDraw->DTextOut(p->Draw->MaxMapX + 28, 433, FPSString, COLOR_WHITE);
                    //  Compile string and assign to FPSString
					sprintf(FPSString, "Ping: %i", p->InGame->TCPPing);
                    //  Draw text at bottom right below previous text using white font color
					p->DDraw->DTextOut(p->Draw->MaxMapX + 105, 433, FPSString, COLOR_WHITE);
				}
                //  Perform bit blocking transfer
				p->DDraw->Flip();
				break;
            //  Display joining game screen
			case STATE_JOINING:
                //  Clear DirectDraw canvas
				p->DDraw->Clear();
				p->DDraw->Draw(p->DDraw->imgCompany, (p->ResolutionX/2), (p->ResolutionY/2)-240, 320, 240);
				p->DDraw->Draw(p->DDraw->imgCompany, (p->ResolutionX/2)-320, (p->ResolutionY/2), 320, 240);
				p->DDraw->Draw(p->DDraw->imgBCLogo, (p->ResolutionX/2)-320, (p->ResolutionY/2)-240, 320, 240);
				p->DDraw->Draw(p->DDraw->imgBCLogo, (p->ResolutionX/2), (p->ResolutionY/2), 320, 240);
                //  Draw text in the middle of the screen using yellow font color
				p->DDraw->DTextOut((p->ResolutionX/2)-112, (p->ResolutionY/2)-20, "Joining Game - Please Wait", COLOR_YELLOW);
                //  Perform bit blocking transfer
				p->DDraw->Flip();
                //  Slow down refresh since joining can take time to finish
				break;			
			case 0:
			case STATE_LOGIN:
			case STATE_RECOVER:
			case STATE_NEWACCOUNT:
			case STATE_PERSONALITY:
			case STATE_INTERVIEW:
			case STATE_MEETING:
            //  Display default screen and wait 10 milliseconds
			case STATE_VERIFY:
				p->DDraw->Clear();
				p->DDraw->Draw(p->DDraw->imgCompany, (p->ResolutionX/2), (p->ResolutionY/2)-240, 320, 240);
				p->DDraw->Draw(p->DDraw->imgCompany, (p->ResolutionX/2)-320, (p->ResolutionY/2), 320, 240);
				p->DDraw->Draw(p->DDraw->imgBCLogo, (p->ResolutionX/2)-320, (p->ResolutionY/2)-240, 320, 240);
				p->DDraw->Draw(p->DDraw->imgBCLogo, (p->ResolutionX/2), (p->ResolutionY/2), 320, 240);
				this->DrawConnectionStatus();
				p->DDraw->Flip();
				break;

			default:
				break;
		}
        //  Cycle through Input object
		p->Input->Cycle();
        //  Cycle through Winsock object
		p->Winsock->Cycle();
        //  Upon verifying that current time interval is larger than the engine's FPS tick  
		if (p->Tick > p->Engine->FPSTick) {
            //  Upon verifying state is game state refresh the game area
			if (p->State == STATE_GAME) {
				p->InGame->RefreshArea();
			}
            //  Slow-machine warning: judged once per second, only after a few seconds in the game, only while the window
            //  is focused (a throttled background tab is not a slow computer), at most once per session. Never in the
            //  browser, where the browser decides how often frames run.
			static int secondsInGame = 0;
			static bool warned = false;
			slowWarningDue = false;
			if (p->State == STATE_GAME && p->Input->HasFocus) {
				secondsInGame++;
#ifndef __EMSCRIPTEN__
				if (!warned && secondsInGame > 5 && p->Engine->FPS < 10 && lastFPS < 10 && lastlastFPS < 10) {
					warned = true;
					slowWarningDue = true;
				}
#endif
			} else {
				secondsInGame = 0;
			}
            //  Set lastlastFPS to lastFPS
			lastlastFPS = lastFPS;
            //  Set lastFPS to Engine->FPS
			lastFPS = p->Engine->FPS;
            //  Set FPS to Engine->loops
			p->Engine->FPS = p->Engine->loops;
            //  Set FPSTick to time interval plus 1000
			p->Engine->FPSTick = p->Tick + 1000;
            //  Set loops to zero
			p->Engine->loops = 0;
		}
        //  Update the 3d sound engine
		p->Sound->Update();
        //  Increase loops by one
		p->Engine->loops++;
        //  Verify limitfps is defined
		if (p->Options->limitfps) {
            //  Upon verifying engine FPS is less than 10, lastlastFPS is less than 10, 
            //  and state is STATE_GAME display message box alerting user that the system 
            //  cannot handle the game
            //  Upon verifying that FPS is greater than 30 set next frame to time interval plus 1
            //  Upon verifying that FPS is greater than 50 set next frame to time interval plus 6
            //  Upon verifying that FPS is greater than 60 set next frame to time interval plus 8
            //  Upon verifying that FPS is greater than 70 set next frame to time interval plus 10
            //  Otherwise set next time to time interval plus 12
			if (slowWarningDue) {
				slowWarningDue = false;
				BCMessageBox(p->hWnd, "Your computer is running too slow to play BattleCity.  Try closing some other programs or updating your video card drivers.", "BattleCity", 0);
				NextFrame = p->Tick + 12;
			} else if (p->Engine->FPS < 30 && lastFPS < 30 && lastlastFPS < 30) {
				NextFrame = p->Tick + 1;
			} else if (p->Engine->FPS < 50 && lastFPS < 50) {
				NextFrame = p->Tick + 6;
			} else if (p->Engine->FPS < 60 && lastFPS < 60 && lastlastFPS < 60) {
				NextFrame = p->Tick + 8;
			} else if (p->Engine->FPS < 70 && lastFPS < 70 && lastlastFPS < 70) {
				NextFrame = p->Tick + 10;
			} else {
				NextFrame = p->Tick + 12;
			}
            //  (the wait until NextFrame happens at the top of Frame(), without blocking)
		}
}

/// <summary>   While there is no connection to the server, says why and when the next attempt is </summary>
void CEngine::DrawConnectionStatus() {
	if (!p->Winsock->Status.empty()) {
		p->DDraw->DTextOut((p->ResolutionX/2)-320, (p->ResolutionY/2)+250, p->Winsock->Status, COLOR_YELLOW);
	} else if (p->Winsock->Connecting) {
		p->DDraw->DTextOut((p->ResolutionX/2)-320, (p->ResolutionY/2)+250, "Connecting to the server...", COLOR_YELLOW);
	}
}

/// <summary>   Splash screen + dialogs while the engine is not running (e.g. connection failed) </summary>
void CEngine::IdleFrame() {
	p->Dialog->Cycle();
	p->DDraw->Clear();
	p->DDraw->Draw(p->DDraw->imgCompany, (p->ResolutionX/2), (p->ResolutionY/2)-240, 320, 240);
	p->DDraw->Draw(p->DDraw->imgCompany, (p->ResolutionX/2)-320, (p->ResolutionY/2), 320, 240);
	p->DDraw->Draw(p->DDraw->imgBCLogo, (p->ResolutionX/2)-320, (p->ResolutionY/2)-240, 320, 240);
	p->DDraw->Draw(p->DDraw->imgBCLogo, (p->ResolutionX/2), (p->ResolutionY/2), 320, 240);
	this->DrawConnectionStatus();
	p->DDraw->Flip();
}

/// <summary>   Log errors to debug.log </summary>
///
/// <param name="error">    The error. </param>
void CEngine::logerror(std::string error) {
    //  Prepare file handler
	FILE *file;
    //  Open debug.log through file handler
	file = fopen("debug.log","a");
    //  Set linebreak to be 3 characters
	char linebreak[3];
    //  Set carriage return
	linebreak[0] = 13;
    //  Set line feed
	linebreak[1] = 10;
    //  Set Null character
	linebreak[2] = 0;
    //  Set ErrorString to be 255 characters
	char ErrorString[255];
    //  Copy characters from error to ErrorString
	strcpy(ErrorString, error.c_str());
    //  Send error string to game server
	p->Winsock->SendData(cmCrash, ErrorString);
    //  Upon verifying state is STATE_GAME populate chat with error string
	if (p->State == STATE_GAME) {
		error = "WARNING:  Game Crash.  Code:  " + error;
        //  Populate chat with error string
		p->InGame->AppendChat(error.c_str(), COLOR_YELLOW);
        //  Turn sound off
		p->Options->sound = 0;
	}
    //  Append linebreak
	error = linebreak + error;
    //  Write error message to file handler
	fwrite(error.c_str(), error.size(), 1, file); 
    //  Close file handler
	fclose(file);
    //  Clear error object
	error.clear();
}

void CEngine::ThreadMessage(std::string MsgText) {
	if (this->MsgUp == 1) return;
	this->MsgText = MsgText;
	this->MsgUp = 1;
	MessageBox(p->hWnd, this->MsgText.c_str(), "BattleCity", 0);
	this->MsgUp = 0;
	this->MsgQueue = 0;
}

void CEngine::ThreadMessageQuit() {
	if (this->MsgUp == 1) return;
	this->MsgUp = 1;
	BCMessageBoxAsync("Leave BattleCity?", "BattleCity", MB_YESNO, [this](int response) {
		if (response == 1) {
			p->State = STATE_MEETING;
			p->Meeting->ShowMeetingDlg();
		}
		this->MsgUp = 0;
	});
}

/// <summary> Diagnostic note: debug.log + the server log (at most one note per kind every 20 s); never chat, never mutes sound </summary>
void CEngine::lognote(std::string note) {
	FILE *file = fopen("debug.log", "a");
	if (file) {
		fprintf(file, "\n%s", note.c_str());
		fclose(file);
	}
	static std::map<std::string, unsigned long> lastSent;
	std::string kind = note.substr(0, 24);
	unsigned long now = BCGetTicks();
	if (lastSent.count(kind) && now - lastSent[kind] < 20000) return;
	lastSent[kind] = now;
	char text[255];
	snprintf(text, sizeof(text), "%s", note.c_str());
	if (p->Winsock->Connected) p->Winsock->SendData(cmCrash, text);
}
