# Battle City network protocol

All traffic is plain TCP on port 9680 (`BC_PORT` overrides on the server). The browser client reaches the same port through a WebSocket gateway (`websockify`, see `deploy/web/`), which forwards the byte stream unchanged. Both sides set `TCP_NODELAY`: position updates are small and must not wait for Nagle's algorithm.

The definitions live in `server/NetMessages.h` and `client/NetMessages.h` (they must stay identical). This page is generated from that header.

## Framing

Every packet, in either direction:

| Byte | Meaning |
|------|---------|
| 0 | `length`: number of bytes that follow this one (`payload size + 2`, at most 250) |
| 1 | `checksum`: `(sum of payload bytes as signed chars + 3412) % 71` |
| 2 | packet id (`sm*` from the server, `cm*` from the client) |
| 3... | payload: a packed struct of fixed size, or a zero-terminated string |

A receiver reads one length byte, waits until the whole packet has arrived, verifies the checksum and drops the packet on mismatch. Payloads are zero-padded on receipt, so a short packet reads as zeros. Integers are little-endian; structs use the widths in the header (`uint32_t`, `unsigned short`, ...), never `long`.

## Rules the server enforces

* Packets that only make sense in a game (`cmBuild`, `cmUpdate`, `cmShoot`, `cmFired`, `cmItemUp`, `cmItemDrop`, `cmDeath`, `cmDemolish`, `cmMedKit`, `cmHitObject`, `cmSuccessor`, `cmSetMayor`, `cmAutoBuild`, `cmCloak`) are ignored unless the player is in a game.
* Every player index, city index, building type and map position taken from a packet is range-checked before use. Strings are length-bounded and SQL-escaped.
* `cmUpdate` positions more than 300 units from the server's last position are rejected and the client is warped back (`smWarp`).

## Movement updates

* The client sends `cmUpdate` (`x`, `y`, `turn`, `move`, `dir`) every 150 ms and immediately whenever its move or turn input changes.
* The server relays it as `smUpdate` (adds `id`) to players within radar range.
* A receiver carries the reported position forward by half the measured round trip, moves the tank using `dir` and `move`, and blends towards each new update (snapping if it is more than 150 units off).

## Server messages (`sm*`)

| Id | Name |
|---:|------|
| 0 | `SMTCP` |
| 1 | `smLoginCorrect` |
| 2 | `smAppendNews` |
| 3 | `smChatEcho` |
| 4 | `smChatMessage` |
| 5 | `smPlayerData` |
| 6 | `smChatCommand` |
| 7 | `smAddRemCity` |
| 8 | `smStateGame` |
| 9 | `smMayorHire` |
| 10 | `smMayorInInterview` |
| 11 | `smInterviewCancel` |
| 12 | `smMayorDeclined` |
| 13 | `smMayorUpdate` |
| 14 | `smCanBuild` |
| 15 | `smNewBuilding` |
| 16 | `smPlaceBuilding` |
| 17 | `smError` |
| 18 | `smJoinData` |
| 19 | `smComms` |
| 20 | `smFired` |
| 21 | `smFinance` |
| 22 | `smItemCount` |
| 23 | `smAddItem` |
| 24 | `smRemItem` |
| 25 | `smUpdatePop` |
| 26 | `smDeath` |
| 27 | `smOrbed` |
| 28 | `smDestroyCity` |
| 29 | `smDemolish` |
| 30 | `smHP` |
| 31 | `smRemBuilding` |
| 32 | `smInterview` |
| 33 | `smPickedUp` |
| 34 | `smDropped` |
| 35 | `smTCPPong` |
| 36 | `smPointsUpdate` |
| 37 | `smMedKit` |
| 38 | `smExplosion` |
| 39 | `smWarp` |
| 40 | `smNextStep` |
| 41 | `smItemLife` |
| 42 | `smGlobal` |
| 43 | `smPromotion` |
| 44 | `smAdmin` |
| 45 | `smKicked` |
| 46 | `smUpdate` |
| 47 | `smShoot` |
| 48 | `smTurretLook` |
| 49 | `smTurretShoot` |
| 50 | `smSectorSent` |
| 51 | `smEditAccount` |
| 52 | `smInfoButton` |
| 53 | `smClearPlayer` |
| 54 | `smMayorChanged` |
| 55 | `smRightClickCity` |
| 56 | `smRespawn` |
| 57 | `smBan` |
| 58 | `smUnderAttack` |
| 59 | `smAdminEdit` |
| 60 | `smClickPlayer` |
| 61 | `smWhisper` |
| 62 | `smCount` |
| 63 | `smCloak` |
| 64 | `smAutoBuild` |
| 65 | `smNowOrbable` |
| 66 | `smStartingCity` |

## Client messages (`cm*`)

| Id | Name |
|---:|------|
| 0 | `CMTCP` |
| 1 | `cmVersion` |
| 2 | `cmLogin` |
| 3 | `cmNewAccount` |
| 4 | `cmChatMessage` |
| 5 | `cmSetState` |
| 6 | `cmJobApp` |
| 7 | `cmJobCancel` |
| 8 | `cmHireAccept` |
| 9 | `cmHireDecline` |
| 10 | `cmBuild` |
| 11 | `cmComms` |
| 12 | `cmFired` |
| 13 | `cmItemUp` |
| 14 | `cmItemDrop` |
| 15 | `cmDeath` |
| 16 | `cmDemolish` |
| 17 | `cmMedKit` |
| 18 | `cmHitObject` |
| 19 | `cmTCPPing` |
| 20 | `cmCrash` |
| 21 | `cmNextStep` |
| 22 | `cmSuccessor` |
| 23 | `cmGlobal` |
| 24 | `cmAdmin` |
| 25 | `cmBan` |
| 26 | `cmIsHiring` |
| 27 | `cmWalkie` |
| 28 | `cmUpdate` |
| 29 | `cmShoot` |
| 30 | `cmRequestSector` |
| 31 | `cmEditAccount` |
| 32 | `cmAccountUpdate` |
| 33 | `cmRecover` |
| 34 | `cmRequestInfo` |
| 35 | `cmRightClickCity` |
| 36 | `cmChangeNews` |
| 37 | `cmSetMayor` |
| 38 | `cmRefreshList` |
| 39 | `cmMiniMap` |
| 40 | `cmAdminEditRequest` |
| 41 | `cmAdminEdit` |
| 42 | `cmClickPlayer` |
| 43 | `cmChangeTank` |
| 44 | `cmWhisper` |
| 45 | `cmCount` |
| 46 | `cmCloak` |
| 47 | `cmAutoBuild` |
| 48 | `cmCheatCheck` |
| 49 | `cmStartingCity` |
| 50 | `cmChangeStartingCity` |

## Payload structs

Fields in wire order.

* `sSMUpdate`: `unsigned char id`, `unsigned short x`, `unsigned short y`, `unsigned char turn`, `unsigned char move`, `unsigned char dir`
* `sSMItem`: `unsigned short x`, `unsigned short y`, `unsigned char City`, `unsigned char type`, `unsigned char active`, `unsigned short id`
* `sSMFinance`: `uint32_t Cash`, `uint32_t Income`, `uint32_t Research`, `uint32_t Items`, `uint32_t Hospital`
* `sSMItemCount`: `unsigned short id`, `unsigned char count`
* `sSMBuild`: `unsigned char City`, `unsigned short x`, `unsigned short y`, `unsigned char type`, `unsigned char count`, `unsigned short id`, `unsigned char pop`
* `sSMPop`: `unsigned short id`, `unsigned char pop`
* `sSMUpdateTurret`: `unsigned short id`, `unsigned short dir`
* `sSMOrbedCity`: `unsigned char City`, `unsigned char OrberCity`, `unsigned int points`, `unsigned int OrberCityPoints`
* `sSMPoints`: `unsigned char Index`, `unsigned int Points`, `unsigned int Deaths`, `unsigned int Orbs`, `unsigned int Assists`, `unsigned int MonthlyPoints`
* `sSMExplode`: `unsigned char City`, `unsigned short x`, `unsigned short y`
* `sSMMayorUpdate`: `unsigned char Index`, `unsigned char IsMayor`
* `sSMStateGame`: `unsigned short x`, `unsigned short y`, `unsigned char City`
* `sSMJoinData`: `unsigned char id`, `unsigned char Mayor`, `unsigned char City`
* `sSMItemLife`: `unsigned short id`, `unsigned char life`
* `sSMAdmin`: `unsigned short adminIndex`, `unsigned short id`, `unsigned char command`
* `sSMShot`: `unsigned short id`, `unsigned short x`, `unsigned short y`, `unsigned char dir`, `unsigned char type`
* `sSMInfoButton`: `unsigned char city`
* `sSMPlayer`: `char Index`, `char Name[16]`, `char Town[16]`, `unsigned char playerType`, `unsigned char Red`, `unsigned char Green`, `unsigned char Blue`, `unsigned char Member`, `unsigned char Tank`
* `sSMRightClickCity`: `char City`, `unsigned short BuildingCount`, `bool IsOrbable`, `unsigned short Orbs`, `unsigned short OrbPoints`, `unsigned short UptimeInMinutes`
* `sSMClickPlayer`: `char Index`, `unsigned short Orbs`, `unsigned short Assists`, `unsigned short Deaths`
* `sSMBan`: `char Account[16]`, `char IPAddress[16]`, `char Reason[31]`
* `sSMAutoBuild`: `bool isAllowed`, `char filename[64]`
* `sSMStartingCity`: `int City`
* `sCMBuild`: `unsigned short x`, `unsigned short y`, `unsigned char type`, `bool isAutoBuild`
* `sCMUpdate`: `unsigned short x`, `unsigned short y`, `unsigned char turn`, `unsigned char move`, `unsigned char dir`
* `sCMItem`: `unsigned short id`, `char active`, `unsigned char type`
* `sCMDemolish`: `unsigned short id`
* `sCMAdmin`: `unsigned short id`, `unsigned char command`
* `sCMVersion`: `char UniqID[50]`, `char Version[10]`
* `sCMShot`: `unsigned short x`, `unsigned short y`, `unsigned char dir`, `unsigned char type`
* `sCMSector`: `unsigned char x`, `unsigned char y`
* `sCMLogin`: `char User[15]`, `char Pass[15]`
* `sCMNewAccount`: `char User[15]`, `char Pass[15]`, `char Email[50]`, `char FullName[20]`, `char Town[15]`, `char State[15]`
* `sCMAdminEditRequest`: `char User[15]`
* `sCMAdminEdit`: `char User[15]`, `char Pass[15]`, `char Email[50]`, `char FullName[20]`, `char Town[15]`, `char State[15]`, `int Points`, `int MonthlyPoints`, `int Deaths`, `int Orbs`, `int Assists`, `int playerType`, `unsigned char Member`, `unsigned char Red`, `unsigned char Green`, `unsigned char Blue`, `unsigned char Tank`, `unsigned char Tank2`, `unsigned char Tank3`, `unsigned char Tank4`, `unsigned char Tank5`, `unsigned char Tank6`, `unsigned char Tank7`, `unsigned char Tank8`, `unsigned char Tank9`, `int RentalCity`
* `sCMWhisper`: `char Sender`, `char Recipient`, `char Message[128]`
* `sCMAutoBuild`: `bool isAllowed`, `char filename[64]`
* `sCMCheatCheck`: `int maxHealth`, `int costBuilding`, `int damageLaser`, `int damageMine`, `int damageRocket`, `float movementBullet`, `float movementPlayer`, `int timerCloak`, `int timerDfg`, `int timerRespawn`, `int timerLaser`, `int timerRocket`
* `sCMStartingCity`: `int City`
