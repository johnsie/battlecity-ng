/*
===============================================================================

    Battle City - CSound header file
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
#include "CSound.h"
#include <SDL.h>
#include <vector>
#include <cmath>
#include <algorithm>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define TSF_IMPLEMENTATION
#include "../external/tinysoundfont/tsf.h"
#define TML_IMPLEMENTATION
#include "../external/tinysoundfont/tml.h"

namespace {
const int MIX_RATE = 44100;
const int MAX_VOICES = 32;
const int FIRST_FREE_VOICE = 16;     // 0-15: fixed channels used by the game (1, 2, 5, 12, 13 ...)
const int SONG_COUNT = 7;
const char *SONG_FILES[SONG_COUNT] = { "midi/Bc1.mid", "midi/Bc2.mid", "midi/Bc3.mid", "midi/Bc5.mid", "midi/Bc6.mid", "midi/Bc8.mid", "midi/Bc9.mid" };
}

#ifdef __EMSCRIPTEN__
static CSound *g_soundForDownload = NULL;
#endif

/// <summary> Interleaved stereo, 44.1 kHz, signed 16-bit </summary>
struct BCSample {
	std::vector<int16_t> pcm;
	bool loop;
};

struct BCVoice {
	BCSample *sample;
	size_t pos;                      // in sample frames
	float left, right;
	bool active;
};

struct BCAudio {
	SDL_AudioDeviceID device;
	BCVoice voices[MAX_VOICES];
	std::vector<BCSample *> samples;

	tsf *font;
	tml_message *songs[SONG_COUNT];
	tml_message *cursor;             // next MIDI message to play
	double songMs;                   // position in the current song
	unsigned int songLength;         // ms
	bool songPlaying;
	bool songFinished;               // set when a song ends; cleared by CSound::Update()
	float musicGain;
};

namespace {

void RenderMusic(BCAudio *a, float *mix, int frames) {
	if (!a->songPlaying || !a->font) return;
	short block[64 * 2];
	int done = 0;
	while (done < frames) {
		int n = std::min(64, frames - done);
		a->songMs += n * 1000.0 / MIX_RATE;
		while (a->cursor && a->songMs >= a->cursor->time) {
			tml_message *m = a->cursor;
			switch (m->type) {
				case TML_PROGRAM_CHANGE: tsf_channel_set_presetnumber(a->font, m->channel, m->program, m->channel == 9); break;
				case TML_NOTE_ON:        tsf_channel_note_on(a->font, m->channel, m->key, m->velocity / 127.0f); break;
				case TML_NOTE_OFF:       tsf_channel_note_off(a->font, m->channel, m->key); break;
				case TML_PITCH_BEND:     tsf_channel_set_pitchwheel(a->font, m->channel, m->pitch_bend); break;
				case TML_CONTROL_CHANGE: tsf_channel_midi_control(a->font, m->channel, m->control, m->control_value); break;
			}
			a->cursor = m->next;
		}
		tsf_render_short(a->font, block, n, 0);
		for (int i = 0; i < n * 2; i++) mix[done * 2 + i] += block[i] * (1.0f / 32768.0f) * a->musicGain;
		done += n;
	}
	// finished: no messages left and the last notes have had time to ring out
	if (!a->cursor && a->songMs > a->songLength + 1500) {
		a->songPlaying = false;
		a->songFinished = true;
		tsf_note_off_all(a->font);
	}
}

void AudioCallback(void *userdata, Uint8 *stream, int len) {
	BCAudio *a = (BCAudio *)userdata;
	int frames = len / (int)(sizeof(int16_t) * 2);
	static std::vector<float> mix;
	mix.assign((size_t)frames * 2, 0.0f);

	RenderMusic(a, mix.data(), frames);

	for (BCVoice &v : a->voices) {
		if (!v.active) continue;
		const std::vector<int16_t> &pcm = v.sample->pcm;
		size_t total = pcm.size() / 2;
		for (int i = 0; i < frames; i++) {
			if (v.pos >= total) {
				if (v.sample->loop && total > 0) v.pos = 0;
				else { v.active = false; break; }
			}
			mix[(size_t)i * 2]     += pcm[v.pos * 2]     * (1.0f / 32768.0f) * v.left;
			mix[(size_t)i * 2 + 1] += pcm[v.pos * 2 + 1] * (1.0f / 32768.0f) * v.right;
			v.pos++;
		}
	}

	int16_t *out = (int16_t *)stream;
	for (int i = 0; i < frames * 2; i++) {
		float s = std::max(-1.0f, std::min(1.0f, mix[i]));
		out[i] = (int16_t)(s * 32767.0f);
	}
}

} // namespace

CSound::CSound(CGame *game)
{
	p = game;
	MIDIndex = 0;
	audio = NULL;
	s_tanklaser = s_fire = s_engine = s_build = s_die = s_eXplode = s_turret = NULL;
	s_buzz = s_click = s_bigturret = s_demolish = s_screech = s_hit = s_cloak = s_flare = NULL;
}

CSound::~CSound()
{
	Shutdown();
}

BCSample *CSound::LoadSample(const char *file, bool loop)
{
	SDL_AudioSpec spec;
	Uint8 *buffer = NULL;
	Uint32 length = 0;
	if (!SDL_LoadWAV(file, &spec, &buffer, &length)) {
		p->Engine->logerror(std::string("Unable to load sound ") + file + ": " + SDL_GetError());
		return NULL;
	}
	SDL_AudioCVT cvt;
	BCSample *sample = NULL;
	if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 2, MIX_RATE) >= 0) {
		cvt.len = (int)length;
		std::vector<Uint8> work((size_t)length * (cvt.len_mult > 0 ? cvt.len_mult : 1));
		memcpy(work.data(), buffer, length);
		cvt.buf = work.data();
		if (cvt.needed ? SDL_ConvertAudio(&cvt) == 0 : true) {
			int bytes = cvt.needed ? cvt.len_cvt : (int)length;
			sample = new BCSample;
			sample->loop = loop;
			sample->pcm.assign((int16_t *)work.data(), (int16_t *)work.data() + bytes / 2);
		}
	}
	SDL_FreeWAV(buffer);
	if (sample) audio->samples.push_back(sample);
	return sample;
}

void CSound::Init()
{
	if (audio) return;
	audio = new BCAudio();
	audio->device = 0;
	audio->font = NULL;
	audio->cursor = NULL;
	audio->songMs = 0;
	audio->songLength = 0;
	audio->songPlaying = false;
	audio->songFinished = false;
	audio->musicGain = 0.75f;
	for (BCVoice &v : audio->voices) v.active = false;
	for (int i = 0; i < SONG_COUNT; i++) audio->songs[i] = NULL;

	// Decode everything first, then open the device (so the callback never sees half-built state)
	s_engine = LoadSample("wav/engine.wav", true);
	s_tanklaser = LoadSample("wav/laser.wav", false);
	s_fire = LoadSample("wav/fire.wav", false);
	s_build = LoadSample("wav/build.wav", false);
	s_die = LoadSample("wav/die.wav", false);
	s_eXplode = LoadSample("wav/explode.wav", false);
	s_turret = LoadSample("wav/turretfire.wav", false);
	s_buzz = LoadSample("wav/buzz.wav", false);
	s_click = LoadSample("wav/click.wav", false);
	s_bigturret = LoadSample("wav/bturr.wav", false);
	s_demolish = LoadSample("wav/demo.wav", false);
	s_screech = LoadSample("wav/screech.wav", false);
	s_hit = LoadSample("wav/hit.wav", false);
	s_cloak = LoadSample("cloak.wav", false);
	s_flare = LoadSample("flare.wav", false);

	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = MIX_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 2048;
	want.callback = AudioCallback;
	want.userdata = audio;
	audio->device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);   // 0: SDL converts if the hardware differs
	if (audio->device == 0) {
		p->Engine->logerror(std::string("No audio device: ") + SDL_GetError());
		return;
	}
	SDL_PauseAudioDevice(audio->device, 0);

	srand((unsigned int)p->Tick);
	this->MIDIndex = rand() % 6;

#ifdef __EMSCRIPTEN__
	// The soundfont is ~6 MB: download it in the background so the game can start immediately
	g_soundForDownload = this;
	emscripten_async_wget("TimGM6mb.sf2", "/TimGM6mb.sf2",
		[](const char *) { if (g_soundForDownload) g_soundForDownload->LoadMusicAssets(); },
		[](const char *) { printf("music: soundfont download failed, music disabled\n"); });
#else
	LoadMusicAssets();
#endif
}

/// <summary> Loads the soundfont and the MIDI files, then starts the music. Safe to call once the soundfont file exists. </summary>
void CSound::LoadMusicAssets()
{
	if (!audio || audio->font) return;
	tsf *font = tsf_load_filename("TimGM6mb.sf2");
	if (!font) {
		p->Engine->logerror("Unable to load the sound font TimGM6mb.sf2: music disabled");
		return;
	}
	tsf_set_output(font, TSF_STEREO_INTERLEAVED, MIX_RATE, 0.0f);
	tml_message *songs[SONG_COUNT];
	for (int i = 0; i < SONG_COUNT; i++) songs[i] = tml_load_filename(SONG_FILES[i]);
	if (audio->device) SDL_LockAudioDevice(audio->device);
	for (int i = 0; i < SONG_COUNT; i++) audio->songs[i] = songs[i];
	audio->font = font;
	if (audio->device) SDL_UnlockAudioDevice(audio->device);
	printf("music: soundfont loaded, starting song %d\n", MIDIndex);
	PlaYMID(MIDIndex);
}

void CSound::Shutdown()
{
	if (!audio) return;
	if (audio->device) SDL_CloseAudioDevice(audio->device);
	if (audio->font) tsf_close(audio->font);
	for (int i = 0; i < SONG_COUNT; i++) if (audio->songs[i]) tml_free(audio->songs[i]);
	for (BCSample *s : audio->samples) delete s;
	delete audio;
	audio = NULL;
}

void CSound::Play(BCSample *sample, int channel, float left, float right)
{
	if (!audio || !audio->device || !sample) return;
	SDL_LockAudioDevice(audio->device);
	int slot = channel;
	if (slot < 0 || slot >= MAX_VOICES) {
		slot = -1;
		for (int i = FIRST_FREE_VOICE; i < MAX_VOICES; i++) if (!audio->voices[i].active) { slot = i; break; }
		if (slot < 0) slot = FIRST_FREE_VOICE + (int)(SDL_GetTicks() % (MAX_VOICES - FIRST_FREE_VOICE));   // all busy: steal one
	}
	BCVoice &v = audio->voices[slot];
	v.sample = sample;
	v.pos = 0;
	v.left = left;
	v.right = right;
	v.active = true;
	SDL_UnlockAudioDevice(audio->device);
}

void CSound::PlayWav(int Index, int channel)
{
	if (p->Options->sound != 1) return;
	BCSample *table[] = { s_tanklaser, s_fire, s_engine, s_build, s_die, s_eXplode, s_turret, s_buzz, s_click, s_bigturret, s_demolish, s_screech, s_hit, s_cloak, s_flare };
	if (Index < 0 || Index >= 15) return;
	const float gain = 0.7f;
	Play(table[Index], channel, gain, gain);
}

void CSound::Play3dSound(BCSample *sound, int volume, float x, float y)
{
	if (p->Options->sound != 1 || !sound) return;
	// Position relative to the local player, scaled so that +-1 is the edge of hearing range
	float dx = (x - p->Player[p->Winsock->MyIndex]->X) / 2400.0f;
	float dy = (y - p->Player[p->Winsock->MyIndex]->Y) / 2400.0f;
	if (dx < -1.0f || dx > 1.0f || dy < -1.0f || dy > 1.0f) return;   // out of hearing range
	float dist = std::sqrt(dx * dx + dy * dy);
	const float minDist = 0.15f;
	float attenuation = dist <= minDist ? 1.0f : minDist / dist;       // inverse-distance rolloff
	float gain = (volume / 100.0f) * 0.7f * attenuation;
	float pan = std::max(-1.0f, std::min(1.0f, dx));                   // -1 = left, +1 = right
	// constant-power pan, but never fully silence one ear
	float left = gain * std::cos((pan + 1.0f) * 0.25f * 3.14159265f) * 1.41f;
	float right = gain * std::sin((pan + 1.0f) * 0.25f * 3.14159265f) * 1.41f;
	Play(sound, -1, left, right);
}

void CSound::StopWav(int Index)
{
	if (!audio || !audio->device || Index < 0 || Index >= MAX_VOICES) return;
	SDL_LockAudioDevice(audio->device);
	audio->voices[Index].active = false;
	SDL_UnlockAudioDevice(audio->device);
}

void CSound::PlaYMID(int Index)
{
	if (!audio || !audio->device || !audio->font || p->Options->music != 1) return;
	if (Index < 0 || Index >= SONG_COUNT || !audio->songs[Index]) return;
	SDL_LockAudioDevice(audio->device);
	tsf_reset(audio->font);
	tsf_channel_set_presetnumber(audio->font, 9, 0, 1);       // channel 10 is percussion
	audio->cursor = audio->songs[Index];
	audio->songMs = 0;
	audio->songLength = 0;
	tml_get_info(audio->songs[Index], NULL, NULL, NULL, NULL, &audio->songLength);
	audio->songFinished = false;
	audio->songPlaying = true;
	SDL_UnlockAudioDevice(audio->device);
}

void CSound::StopMID(int all, int Index)
{
	if (!audio || !audio->device) return;
	SDL_LockAudioDevice(audio->device);
	audio->songPlaying = false;
	audio->songFinished = false;
	if (audio->font) tsf_note_off_all(audio->font);
	SDL_UnlockAudioDevice(audio->device);
}

void CSound::Update()
{
	if (audio && audio->songFinished) musicCycle();
}

int CSound::musicCycle()
{
	if (!audio || !audio->songFinished) return 0;
	audio->songFinished = false;
	MIDIndex++;
	if (MIDIndex >= SONG_COUNT) MIDIndex = 0;
	PlaYMID(MIDIndex);
	return 0;
}
