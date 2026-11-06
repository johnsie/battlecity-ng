# Voice chat

Players can talk to each other. It is **off until a player turns it on**, and turning it on asks the browser (or the system) for the microphone. Nothing is recorded or stored.

Voice goes through the game server, over the connection the game already has: no extra server, no open ports, and players never connect to each other (nobody sees another player's address). It was modelled on the voice chat in Jimmy's Flying Circus.

## For players
- **V** turns voice on and off while you play. The line under the info text at the top left shows the state ("Voice on: hold ` to talk (Everyone)"), and who is talking is listed under it (">> Name"). **Click that line** to open the settings.
- **Settings** are in **Options > Voice chat...** (or by clicking the voice line). They are kept on your device:
  - **When you talk:** *Push to talk* (hold the **`** key, the one left of 1) or *Open mic* (you are heard when you speak, with a sensitivity setting).
  - **Who hears you:** *Everyone* (the default, so two players who both turn voice on always hear each other), *My city only* or *Players nearby only* (about 37 tiles). Two players hear each other only if each one's choice includes the other, so someone on "My city only" is never heard by another city.
  - **Volume**, **Mute everyone**, and for any player in the list: **Mute / unmute** (this session), **Block** (remembered by name) and **Report** (written to `voice-reports.log` on the server).
- If the microphone is refused you can still listen. Use headphones to avoid echo.

## How it works (and why it does not affect play)
- **Capture** (`client/CVoice.cpp`): SDL records 16 kHz mono sound on its own audio thread, cuts it into 20 ms frames and squeezes them with IMA ADPCM (`client/adpcm.h`, 64 kbit/s) only while you are talking. A frame is a 165-byte packet (`cmVoice`: sequence number, predictor, step index, audio), which fits the game's 255-byte packet limit. The game thread only sends what the audio thread has prepared (at most six packets a frame); if it falls behind, the oldest audio is dropped instead of piling up.
- **Server** (`server/CVoice.cpp`): checks the packet, applies the rules above and forwards it to each listener as `smVoice` with the speaker's slot. It does not decode, mix or store audio. Gameplay always comes first:
  - a player who sends more than 70 packets a second (50 is normal) is ignored until they calm down;
  - a listener whose connection is more than 16 KB behind simply misses voice, so voice never delays game packets;
  - malformed packets (shorter than 6 or longer than 200 bytes) are dropped, and only players in the game with voice on can send or hear.
- **Playback**: each speaker has a 100 ms jitter buffer, is resampled to 44.1 kHz and mixed into the game's own audio callback (`CSound.cpp`), so voice and game sound share one device.
- **Protocol** (appended to the end of the enums, so old clients are unaffected): client `cmVoiceSet [on][scope]`, `cmVoice [audio packet]`, `cmVoiceReport [slot]`; server `smVoiceState [enabled][on][scope]` (sent when a player joins and after every change) and `smVoice [speaker][audio packet]`.
- `BC_VOICE=0` on the server switches voice off; clients then show no voice line and `V` says it is not available.

## Cost
About 70 kbit/s (with packet headers) from each person talking, and the same to each listener. Only people who are talking use bandwidth.

## Phones and browsers
Browsers need https for the microphone (the live site has it). If the game is embedded in an iframe, the iframe needs `allow="microphone"`. The microphone is requested when voice is turned on; a page in the background may stop sending.

## Tests
- `ci/voice-test.sh <bc-server> <map.dat>` (`tools/voice_test.py`): players are told voice is available, a listener hears the speaker with the audio unchanged, the speaker never hears themselves, voice off hears and is heard by nobody, "my city" keeps other cities out, malformed packets and floods are dropped, and turning voice off stops the audio.
- The desktop client has a self test: with `BC_TEST_SCENE=1 BC_TEST_VOICE=1` a 440 Hz tone goes through capture, the codec and the mixer and must come out as that tone.
