# Battle City
---

  Battle City is a very fun and addicting online game. You start off by either building a city or applying for a job in a city built by someone else. Your goal is to defend your city while attacking and destroying enemy cities. All of this is done in real-time from the comfort of your tank! For more information, visit the Official Battle City Website.
  The best thing about Battle City is that it has been released under the GPLv3 open source license. This license gives ownership of the game to the community and ensures Battle City will stay free and open source forever!
  Help Battle City by playing the game, updating the wiki, or contributing code!

License: GPLv3
Credits: Deceth

Download the latest release from the [official website](http://battlecity.org)

Download the source code from the master [Battle-City code repository](https://github.com/Deceth/Battle-City)

## Resources
* [How to Setup your Development Environment](https://github.com/Deceth/Battle-City/wiki/How-to-Setup-your-Development-Environment)
* [Game Design](https://github.com/Deceth/Battle-City/wiki#game-design)



How to Setup your Development Environment
rileyw edited this page on Apr 3, 2013 · 2 revisions
johnsie edited this on 26/08/16
Seemed to compile out of the box in Visual Studio Community edition 2018. Allthough I may have already had the prerequisites on the machine beforehand.



At this time development must be completed on a Windows machine due to the library dependencies.

Installing applications and dependencies
Battle City requires that you have the following applications and dependencies installed:

Visual C++ (2008/2010 Express)
FMOD 3.75 (Available through this project under the external folder)
DirectX SDK (Feb. 2010)
Download and extract the Battle City source code
You can download the source code by going to the project homepage and using the "Zip" button at the top of the page. Extract the source code to a local, accessible directory.

Open Battle City Solutions File
Navigate to the local, accessible directory where the Battle City source code was extracted. Open the BattleCity.sln file. Visual C++ should open with the Battle City Solution active.

Checking configuration
Right-Click on "BattleCity" from the Solutions Explorer

Click on Properties

Expand "Configuration Properties"

Click on "C/C++"

Set "Additional Include Directories" to point to the following:

 fmod-3.75\api\inc
Click on "Linker"

Set "Additional Library Dependencies" to point to the following:

 DirectX...\Lib\x86\
 fmod-3.75\api\lib\
Expand "Linker"

Click on "Input"

Confirm the following items are listed:

dxguid.lib
ddraw.lib
dinput8.lib
ws2_32.lib
fmodvc.lib
winmm.lib
comctl32.lib
Obtain other assets such as music, sound, maps, graphics, etc... from distribution

Extract other assets into the \Debug folder

Ready to build
