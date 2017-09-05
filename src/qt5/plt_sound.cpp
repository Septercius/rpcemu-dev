/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2017 Peter Howkins

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <assert.h>
#include <stdint.h>

#include <iostream>

#include <QAudioFormat>
#include <QAudioOutput>
#include <QFile>
#include <QThread>

#include "rpcemu.h"
#include "plt_sound.h"

/* All these functions need to be callable from sound.c */
extern "C" void plt_sound_init(uint32_t bufferlen);
extern "C" void plt_sound_restart(void);
extern "C" void plt_sound_pause(void);
extern "C" int32_t plt_sound_buffer_free(void);
extern "C" void plt_sound_buffer_play(uint32_t samplerate, const char *buffer, uint32_t length);

AudioOut *audio_out; /**< Our class used to hold QT sound variables */

/**
 * Our class constructor
 * 
 * @param bufferlen size of buffer in bytes of one chunk of audio data that we will be asked to play
 */
AudioOut::AudioOut(uint32_t bufferlen)
{
	audio_output = NULL;
	audio_io = NULL;

	this->bufferlen = bufferlen;
	this->samplerate = 0;

	// Output some information to the log
	QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
	rpclog("plt_sound: qt5 Audio Device: %s\n", info.deviceName().toLocal8Bit().constData());

	QStringList codecs = info.supportedCodecs();
	rpclog("plt_sound: qt5 Audio Codecs Supported: %d\n", codecs.size());
	for(int i = 0; i < codecs.size(); i++) {
		rpclog("%d: %s\n", i, codecs.at(i).toLocal8Bit().constData());
	}

	QList<int> samprates = info.supportedSampleRates();
	rpclog("plt_sound: qt5 Audio SampleRates Supported: %d\n", samprates.size());
	for(int i = 0; i < samprates.size(); i++) {
		rpclog("%d: %d\n", i, samprates.at(i));
	}
}

AudioOut::~AudioOut()
{
}

/**
 * Change from playing back whatever rate we were playing (or not playing anything)
 * to the requested sample rate
 *
 * @param samplerate new samplerate in Hz
 */
void
AudioOut::changeSampleRate(uint32_t samplerate)
{
	QAudioFormat format; /**< Qt output representing a kind of audio format */

	this->samplerate = samplerate;

	// Destroy any previous sound output
	if(audio_output != NULL) {
		delete audio_output;
	}

	// Set the format
	format.setSampleRate(samplerate);
	format.setChannelCount(2);         // Stereo
	format.setSampleSize(16);          // 16 bit sound
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::SignedInt);

	audio_output = new QAudioOutput(format);
	if(QAudio::NoError != audio_output->error()) {
		error("plt_sound: Failed to create QAudioOutput, no audio\n");
		delete audio_output;
		audio_output = NULL;
		return;
	}

	// Verify the format we were given is usable
	QAudioFormat checkFormat = audio_output->format();
	if((int) samplerate != checkFormat.sampleRate()) {
		rpclog("plt_sound: Tried to set sample rate %uHz but was given %dHz, audio may be distorted\n", samplerate, checkFormat.sampleRate());
	}

	audio_output->setCategory("RPCEmu"); // String used in OS Mixer

	if(config.soundenabled) {
		audio_output->setVolume(1.0f);
	} else {
		audio_output->setVolume(0.0f);
	}

	// Set qt buffer len to greater than buffers we are placing into it
	// this prevents buffer underruns but will add latency
	audio_output->setBufferSize(bufferlen * 4);

	audio_io = audio_output->start();
}

/**
 * Called on program startup to initialise the sound system
 * 
 * @param bufferlen Size in bytes of one audio chunk that will be written
 */
void
plt_sound_init(uint32_t bufferlen)
{
	/* Use our class to do the work */
	audio_out = new AudioOut(bufferlen);
	if(NULL == audio_out) {
		fatal("plt_sound_init: out of memory");
	}
}

/**
 * Called when the user turns the sound on via the GUI
 */
void
plt_sound_restart(void)
{
	assert(audio_out);
	assert(config.soundenabled);

	if(audio_out->audio_output) {
		audio_out->audio_output->setVolume(1.0f);
	}
}

/**
 * Called when the user turns the sound off via the GUI
 */
void
plt_sound_pause(void)
{
	assert(audio_out);
	assert(!config.soundenabled);

	if(audio_out->audio_output) {
		audio_out->audio_output->setVolume(0.0f);
	}
}

/**
 * Return the amount of space free in the platforms audio
 * buffer, enables the sound code to see if there's space to
 * write a whole chunk in
 * 
 * @returns Number of bytes free in platform audio buffer
 */
int32_t
plt_sound_buffer_free(void)
{
	assert(audio_out);

	if(audio_out->audio_output) {
		return audio_out->audio_output->bytesFree();
	} else {
		// The first time around we don't have an audio_output yet
		// that'll be created by plt_sound_buffer_play(), so we must
		// return that we can eat a buffer here else that'll never be called
		return audio_out->bufferlen;
	}
}

/**
 * Write some audio data into this platforms audio output 
 * 
 * @thread sound 
 * @param samplerate Frequency in Hz of this block of audio data
 * @param buffer pointer to audio data
 * @param length size of data in bytes
 */
void
plt_sound_buffer_play(uint32_t samplerate, const char *buffer, uint32_t length)
{
	assert(audio_out);
	assert(buffer);
	assert(length > 0);

	if(samplerate != audio_out->samplerate) {
		rpclog("plt_sound: changing to samplerate %uHz\n", samplerate);
		audio_out->changeSampleRate(samplerate);
	}

	if(audio_out->audio_io) {
		audio_out->audio_io->write(buffer, (qint64) length);
	}
}

