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
extern "C" void plt_sound_buffer_play(const char *buffer, uint32_t length);

 
QAudioFormat format; /**< Qt output representing a kind of audio format */

AudioOut *audio_out; /**< Our class used to hold QT sound variables */

/**
 * Our class constructor
 * create an audio stream of the correct format and start it playing
 */
AudioOut::AudioOut(uint32_t bufferlen)
{

	// Set the initial format
	format.setSampleRate(44100);       // 44100 for rpc 16 bit sound
	format.setChannelCount(2);         // Stereo
	format.setSampleSize(16);          // 16 bit sound
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::SignedInt);

	// Check the format is playable on the default device
	QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
	rpclog("Audio Device: %s\n", info.deviceName().toLocal8Bit().constData());

	QStringList codecs = info.supportedCodecs();
	rpclog("Audio Codecs Supported: %d\n", codecs.size());
	for(int i = 0; i < codecs.size(); i++) {
		rpclog("%d: %s\n", i, codecs.at(i).toLocal8Bit().constData());
	}

	if(!info.isFormatSupported(format)) {
		// TODO this shouldn't be fatal, it could just be they need to install the codecs package
		fatal("Unsupported Audio format for playback");
	}

	audio_output = new QAudioOutput(format);
	audio_output->setCategory("RPCEmu"); // String used in OS Mixer

	if(config.soundenabled) {
		audio_output->setVolume(1.0f);
	} else {
		audio_output->setVolume(0.0f);
	}

	audio_output->setBufferSize(bufferlen);

	audio_io = audio_output->start();
}

AudioOut::~AudioOut()
{
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
}

/**
 * Called when the user turns the sound on via the GUI
 */
void
plt_sound_restart(void)
{
	assert(config.soundenabled);

	audio_out->audio_output->setVolume(1.0f);
}

/**
 * Called when the user turns the sound off via the GUI
 */
void
plt_sound_pause(void)
{
	assert(!config.soundenabled);

	audio_out->audio_output->setVolume(0.0f);
}

/**
 * Return the amount of space free in the platforms audio
 * buffer, enables the sound code to see if there's space to
 * write a whole chunk in
 */
int32_t
plt_sound_buffer_free(void)
{
	return audio_out->audio_output->bytesFree();
}

/**
 * Write some audio data into this platforms audio output 
 * 
 * @thread sound 
 * @param buffer pointer to audio data
 * @param length size of data in bytes
 */
void
plt_sound_buffer_play(const char *buffer, uint32_t length)
{
	assert(buffer);
	assert(length > 0);
	audio_out->audio_io->write(buffer, (qint64) length);
}

