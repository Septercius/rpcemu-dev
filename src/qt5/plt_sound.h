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
#ifndef PLT_SOUND_H
#define PLT_SOUND_H

#include <stdint.h>

#include <QAudioFormat>
#include <QAudioOutput>
#include <QFile>
#include <QObject>
#include <QEventLoop>


class AudioOut : public QObject
{
	Q_OBJECT
public:
	AudioOut(uint32_t bufferlen);
	virtual ~AudioOut();
	void changeSampleRate(uint32_t samplerate);

	QAudioOutput *audio_output;
	QIODevice *audio_io;
	uint32_t samplerate;
	uint32_t bufferlen;
};

#endif // PLT_SOUND_H
