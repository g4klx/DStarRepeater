/*
 *	Copyright (C) 2009,2010,2015 by Jonathan Naylor, G4KLX
 *	Copyright (C) 2014 by John Wiseman, G8BPQ
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

#ifndef	SoundCardReaderWriter_H
#define	SoundCardReaderWriter_H

#include "AudioCallback.h"
#include "StdCompat.h"

#include <atomic>
#include <vector>

/*
 * CSoundCardReaderWriter - Platform-abstracted stereo audio I/O for the sound-card modem.
 *
 * Two implementations are selected at compile time:
 *
 *   PortAudio (Windows and macOS):
 *     A single PaStream runs a combined input/output callback (CSoundCardReaderWriter::callback).
 *     The callback receives captured samples and provides playback samples in the same
 *     invocation, which gives the lowest latency on those platforms.
 *     Device enumeration returns human-readable names from the PortAudio device list.
 *
 *   ALSA (Linux):
 *     Separate CSoundCardReader and CSoundCardWriter objects each own an ALSA PCM handle
 *     and run in dedicated threads (Reader::entry() / Writer::entry()).
 *     The reader thread calls IAudioCallback::readCallback() with captured float samples.
 *     The writer thread calls IAudioCallback::writeCallback() to request output samples.
 *     Threading allows independent blocking on capture and playback devices, which is
 *     important when RX and TX use different physical sound cards.
 *     Device enumeration walks the ALSA control interface (snd_card_next / snd_ctl_pcm_next_device).
 *
 * In both cases, samples are normalised floats in [-1.0, 1.0].  The ALSA path converts
 * to/from signed 16-bit PCM internally.
 *
 * IAudioCallback::readCallback(input, n, id) - n float samples arrived from device id.
 * IAudioCallback::writeCallback(output, n, id) - fill n float samples for device id.
 */

#if defined(_WIN32) || (defined(__APPLE__) && defined(__MACH__))

#include "portaudio.h"

class CSoundCardReaderWriter {
public:
	CSoundCardReaderWriter(const std::string& readDevice, const std::string& writeDevice, unsigned int sampleRate, unsigned int blockSize);
	~CSoundCardReaderWriter();

	void setCallback(IAudioCallback* callback, int id);
	bool open();
	void close();

	void callback(const float* input, float* output, unsigned int nSamples);

	static std::vector<std::string> getReadDevices();
	static std::vector<std::string> getWriteDevices();

private:
	std::string        m_readDevice;
	std::string        m_writeDevice;
	unsigned int       m_sampleRate;
	unsigned int       m_blockSize;
	IAudioCallback*    m_callback;
	int                m_id;
	PaStream*          m_stream;

	bool convertNameToDevices(PaDeviceIndex& inDev, PaDeviceIndex& outDev);
};

#else

#include <alsa/asoundlib.h>
#include <thread>

// ALSA capture thread.  entry() loops on snd_pcm_readi(), converts int16 samples
// to float, and delivers them to the IAudioCallback via readCallback().
class CSoundCardReader {
public:
	CSoundCardReader(snd_pcm_t* handle, unsigned int blockSize, unsigned int channels, IAudioCallback* callback, int id);
	virtual ~CSoundCardReader();

	void start();
	void kill();
	void join();

private:
	void entry();

	snd_pcm_t*        m_handle;
	unsigned int      m_blockSize;
	unsigned int      m_channels;
	IAudioCallback*   m_callback;
	int               m_id;
	std::atomic<bool> m_killed;
	float*            m_buffer;
	short*            m_samples;
	std::thread       m_thread;
};

// ALSA playback thread.  entry() calls writeCallback() to fill float samples,
// converts them to int16, and writes them via snd_pcm_writei().
class CSoundCardWriter {
public:
	CSoundCardWriter(snd_pcm_t* handle, unsigned int blockSize, unsigned int channels, IAudioCallback* callback, int id);
	virtual ~CSoundCardWriter();

	void start();
	void kill();
	void join();

	bool isBusy() const;

private:
	void entry();

	snd_pcm_t*        m_handle;
	unsigned int      m_blockSize;
	unsigned int      m_channels;
	IAudioCallback*   m_callback;
	int               m_id;
	std::atomic<bool> m_killed;
	float*            m_buffer;
	short*            m_samples;
	std::thread       m_thread;
};

class CSoundCardReaderWriter {
public:
	CSoundCardReaderWriter(const std::string& readDevice, const std::string& writeDevice, unsigned int sampleRate, unsigned int blockSize);
	~CSoundCardReaderWriter();

	void setCallback(IAudioCallback* callback, int id);
	bool open();
	void close();

	bool isWriterBusy() const;

	static std::vector<std::string> getReadDevices();
	static std::vector<std::string> getWriteDevices();

private:
	std::string          m_readDevice;
	std::string          m_writeDevice;
	unsigned int         m_sampleRate;
	unsigned int         m_blockSize;
	IAudioCallback*      m_callback;
	int                  m_id;
	CSoundCardReader*    m_reader;
	CSoundCardWriter*    m_writer;

	static std::vector<std::string> m_readDevices;
	static std::vector<std::string> m_writeDevices;
};

#endif

#endif
