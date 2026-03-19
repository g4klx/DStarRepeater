/*
 *   Copyright (C) 2006-2010,2015 by Jonathan Naylor G4KLX
 *   Copyright (C) 2014 by John Wiseman, G8BPQ
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "SoundCardReaderWriter.h"
#include "Logger.h"

#include <cassert>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdio>

#if !defined(_WIN32) && !(defined(__APPLE__) && defined(__MACH__))

std::vector<std::string> CSoundCardReaderWriter::m_readDevices;
std::vector<std::string> CSoundCardReaderWriter::m_writeDevices;

CSoundCardReaderWriter::CSoundCardReaderWriter(const std::string& readDevice, const std::string& writeDevice, unsigned int sampleRate, unsigned int blockSize) :
m_readDevice(readDevice),
m_writeDevice(writeDevice),
m_sampleRate(sampleRate),
m_blockSize(blockSize),
m_callback(nullptr),
m_id(-1),
m_reader(nullptr),
m_writer(nullptr)
{
	assert(sampleRate > 0U);
	assert(blockSize > 0U);
}

CSoundCardReaderWriter::~CSoundCardReaderWriter()
{
}

std::vector<std::string> CSoundCardReaderWriter::getReadDevices()
{
	snd_ctl_t *handle = nullptr;
	snd_pcm_t *pcm = nullptr;
	char NameString[256];

	std::vector<std::string> devices(m_readDevices);

	snd_ctl_card_info_t* info;
	snd_ctl_card_info_alloca(&info);

	snd_pcm_info_t* pcminfo;
	snd_pcm_info_alloca(&pcminfo);

	snd_pcm_hw_params_t* pars;
	snd_pcm_hw_params_alloca(&pars);

	unsigned min, max;
	int err;
	snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;

	int card = -1;
	while (::snd_card_next(&card) == 0 && card >= 0) {
		char hwdev[80];
		::snprintf(hwdev, sizeof(hwdev), "hw:%d", card);

		if (::snd_ctl_open(&handle, hwdev, 0) < 0)
			continue;

		::snd_ctl_card_info(handle, info);
		::snd_ctl_card_info_get_name(info);

		int dev = -1;
		while (::snd_ctl_pcm_next_device(handle, &dev) == 0 && dev >= 0) {
			::snd_pcm_info_set_device(pcminfo, dev);
			::snd_pcm_info_set_subdevice(pcminfo, 0);
			::snd_pcm_info_set_stream(pcminfo, stream);

			err = ::snd_ctl_pcm_info(handle, pcminfo);
			if (err != -ENOENT) {
				::snprintf(hwdev, sizeof(hwdev), "hw:%d,%d", card, dev);

				if (::snd_pcm_open(&pcm, hwdev, stream, SND_PCM_NONBLOCK) < 0)
					continue;

				::snd_pcm_hw_params_any(pcm, pars);
				::snd_pcm_hw_params_get_channels_min(pars, &min);
				::snd_pcm_hw_params_get_channels_max(pars, &max);

				::snd_pcm_hw_params_get_rate_min(pars, &min, nullptr);
				::snd_pcm_hw_params_get_rate_max(pars, &max, nullptr);

				::snprintf(NameString, sizeof(NameString), "hw:%d,%d %s(%s)",
					card, dev,
					::snd_pcm_info_get_name(pcminfo),
					snd_ctl_card_info_get_name(info));

				devices.push_back(std::string(NameString));

				::snd_pcm_close(pcm);
				pcm = nullptr;
			}
		}

		::snd_ctl_close(handle);
	}

	return devices;
}

std::vector<std::string> CSoundCardReaderWriter::getWriteDevices()
{
	snd_ctl_t *handle = nullptr;
	snd_pcm_t *pcm = nullptr;
	char NameString[256];

	std::vector<std::string> devices(m_writeDevices);

	snd_ctl_card_info_t* info;
	snd_ctl_card_info_alloca(&info);

	snd_pcm_info_t* pcminfo;
	snd_pcm_info_alloca(&pcminfo);

	snd_pcm_hw_params_t* pars;
	snd_pcm_hw_params_alloca(&pars);

	unsigned min, max;
	int err;
	snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;

	int card = -1;
	while (::snd_card_next(&card) == 0 && card >= 0) {
		char hwdev[80];
		::snprintf(hwdev, sizeof(hwdev), "hw:%d", card);

		if (::snd_ctl_open(&handle, hwdev, 0) < 0)
			continue;

		::snd_ctl_card_info(handle, info);
		::snd_ctl_card_info_get_name(info);

		int dev = -1;
		while (::snd_ctl_pcm_next_device(handle, &dev) == 0 && dev >= 0) {
			::snd_pcm_info_set_device(pcminfo, dev);
			::snd_pcm_info_set_subdevice(pcminfo, 0);
			::snd_pcm_info_set_stream(pcminfo, stream);

			err = ::snd_ctl_pcm_info(handle, pcminfo);
			if (err != -ENOENT) {
				::snprintf(hwdev, sizeof(hwdev), "hw:%d,%d", card, dev);

				if (::snd_pcm_open(&pcm, hwdev, stream, SND_PCM_NONBLOCK) < 0)
					continue;

				::snd_pcm_hw_params_any(pcm, pars);
				::snd_pcm_hw_params_get_channels_min(pars, &min);
				::snd_pcm_hw_params_get_channels_max(pars, &max);

				::snd_pcm_hw_params_get_rate_min(pars, &min, nullptr);
				::snd_pcm_hw_params_get_rate_max(pars, &max, nullptr);

				::snprintf(NameString, sizeof(NameString), "hw:%d,%d %s(%s)",
					card, dev,
					::snd_pcm_info_get_name(pcminfo),
					::snd_ctl_card_info_get_name(info));

				devices.push_back(std::string(NameString));

				::snd_pcm_close(pcm);
				pcm = nullptr;
			}
		}

		::snd_ctl_close(handle);
	}

	return devices;
}

void CSoundCardReaderWriter::setCallback(IAudioCallback* callback, int id)
{
	assert(callback != nullptr);

	m_callback = callback;

	m_id = id;
}

bool CSoundCardReaderWriter::open()
{
	int err = 0;

	char buf1[100];
	char buf2[100];
	char* ptr;

	// Store the opened devices because ALSA won't enumerate them
	m_readDevices.push_back(m_readDevice);
	m_writeDevices.push_back(m_writeDevice);

	::strncpy(buf1, m_writeDevice.c_str(), sizeof(buf1) - 1);
	buf1[sizeof(buf1) - 1] = '\0';
	::strncpy(buf2, m_readDevice.c_str(), sizeof(buf2) - 1);
	buf2[sizeof(buf2) - 1] = '\0';

	ptr = ::strchr(buf1, ' ');
	if (ptr) *ptr = 0;				// Get Device part of name

	ptr = ::strchr(buf2, ' ');
	if (ptr) *ptr = 0;				// Get Device part of name

	std::string writeDevice(buf1);
	std::string readDevice(buf2);

	snd_pcm_t* playHandle = nullptr;
	if ((err = ::snd_pcm_open(&playHandle, buf1, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		wxLogError("Cannot open playback audio device %s (%s)", writeDevice.c_str(), ::snd_strerror(err));
		return false;
	}

	snd_pcm_hw_params_t* hw_params;
	if ((err = ::snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		wxLogError("Cannot allocate hardware parameter structure (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_any(playHandle, hw_params)) < 0) {
		wxLogError("Cannot initialize hardware parameter structure (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_access(playHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		wxLogError("Cannot set access type (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_format(playHandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		wxLogError("Cannot set sample format (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_rate(playHandle, hw_params, m_sampleRate, 0)) < 0) {
		wxLogError("Cannot set sample rate (%s)", ::snd_strerror(err));
		return false;
	}

	unsigned int playChannels = 1U;

	if ((err = ::snd_pcm_hw_params_set_channels(playHandle, hw_params, 1)) < 0) {
		playChannels = 2U;

		if ((err = ::snd_pcm_hw_params_set_channels(playHandle, hw_params, 2)) < 0) {
			wxLogError("Cannot play set channel count (%s)", ::snd_strerror(err));
			return false;
		}
	}

	if ((err = ::snd_pcm_hw_params(playHandle, hw_params)) < 0) {
		wxLogError("Cannot set parameters (%s)", ::snd_strerror(err));
		return false;
	}

	::snd_pcm_hw_params_free(hw_params);

	if ((err = ::snd_pcm_prepare(playHandle)) < 0) {
		wxLogError("Cannot prepare audio interface for use (%s)", ::snd_strerror(err));
		return false;
	}

	// Open Capture
	snd_pcm_t* recHandle = nullptr;
	if ((err = ::snd_pcm_open(&recHandle, buf2, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		wxLogError("Cannot open capture audio device %s (%s)", readDevice.c_str(), ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		wxLogError("Cannot allocate hardware parameter structure (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_any(recHandle, hw_params)) < 0) {
		wxLogError("Cannot initialize hardware parameter structure (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_access(recHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		wxLogError("Cannot set access type (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_format(recHandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		wxLogError("Cannot set sample format (%s)", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_rate(recHandle, hw_params, m_sampleRate, 0)) < 0) {
		wxLogError("Cannot set sample rate (%s)", ::snd_strerror(err));
		return false;
	}

	unsigned int recChannels = 1U;

	if ((err = ::snd_pcm_hw_params_set_channels(recHandle, hw_params, 1)) < 0) {
		recChannels = 2U;

		if ((err = ::snd_pcm_hw_params_set_channels(recHandle, hw_params, 2)) < 0) {
			wxLogError("Cannot rec set channel count (%s)", ::snd_strerror(err));
			return false;
		}
	}

	if ((err = ::snd_pcm_hw_params(recHandle, hw_params)) < 0) {
		wxLogError("Cannot set parameters (%s)", ::snd_strerror(err));
		return false;
	}

	::snd_pcm_hw_params_free(hw_params);

	if ((err = ::snd_pcm_prepare(recHandle)) < 0) {
		wxLogError("Cannot prepare audio interface for use (%s)", ::snd_strerror(err));
		return false;
	}

	short samples[256];
	for (unsigned int i = 0U; i < 10U; ++i)
		::snd_pcm_readi(recHandle, samples, 128);

	wxLogMessage("Opened %s %s Rate %u", writeDevice.c_str(), readDevice.c_str(), m_sampleRate);

	m_reader = new CSoundCardReader(recHandle,  m_blockSize, recChannels,  m_callback, m_id);
	m_writer = new CSoundCardWriter(playHandle, m_blockSize, playChannels, m_callback, m_id);

	m_reader->start();
	m_writer->start();

	return true;
}

void CSoundCardReaderWriter::close()
{
	if (m_reader != nullptr) {
		m_reader->kill();
		m_reader->join();
	}

	if (m_writer != nullptr) {
		m_writer->kill();
		m_writer->join();
	}
}

bool CSoundCardReaderWriter::isWriterBusy() const
{
	return m_writer->isBusy();
}

CSoundCardReader::CSoundCardReader(snd_pcm_t* handle, unsigned int blockSize, unsigned int channels, IAudioCallback* callback, int id) :
m_handle(handle),
m_blockSize(blockSize),
m_channels(channels),
m_callback(callback),
m_id(id),
m_killed(false),
m_buffer(nullptr),
m_samples(nullptr),
m_thread()
{
	assert(handle != nullptr);
	assert(blockSize > 0U);
	assert(channels == 1U || channels == 2U);
	assert(callback != nullptr);

	m_buffer  = new float[blockSize];
	m_samples = new short[2U * blockSize];
}

CSoundCardReader::~CSoundCardReader()
{
	delete[] m_buffer;
	delete[] m_samples;
}

void CSoundCardReader::start()
{
	m_thread = std::thread(&CSoundCardReader::entry, this);
}

void CSoundCardReader::entry()
{
	wxLogMessage("Starting ALSA reader thread");

	while (!m_killed) {
		snd_pcm_sframes_t ret;
		while ((ret = ::snd_pcm_readi(m_handle, m_samples, m_blockSize)) < 0) {
			if (ret != -EPIPE) {
				wxLogWarning("snd_pcm_readi returned %d (%s)", (int)ret, ::snd_strerror(ret));
			}

			::snd_pcm_recover(m_handle, ret, 1);
		}

		if (m_channels == 1U) {
			for (int n = 0; n < ret; n++)
				m_buffer[n] = float(m_samples[n]) / 32768.0F;
		} else {
			int i = 0;
			for (int n = 0; n < (ret * 2); n += 2)
				m_buffer[i++] = float(m_samples[n + 1]) / 32768.0F;
		}

		m_callback->readCallback(m_buffer, (unsigned int)ret, m_id);
	}

	wxLogMessage("Stopping ALSA reader thread");

	::snd_pcm_close(m_handle);
}

void CSoundCardReader::kill()
{
	m_killed = true;
}

void CSoundCardReader::join()
{
	if (m_thread.joinable())
		m_thread.join();
}

CSoundCardWriter::CSoundCardWriter(snd_pcm_t* handle, unsigned int blockSize, unsigned int channels, IAudioCallback* callback, int id) :
m_handle(handle),
m_blockSize(blockSize),
m_channels(channels),
m_callback(callback),
m_id(id),
m_killed(false),
m_buffer(nullptr),
m_samples(nullptr),
m_thread()
{
	assert(handle != nullptr);
	assert(blockSize > 0U);
	assert(channels == 1U || channels == 2U);
	assert(callback != nullptr);

	m_buffer  = new float[2U * blockSize];
	m_samples = new short[4U * blockSize];
}

CSoundCardWriter::~CSoundCardWriter()
{
	delete[] m_buffer;
	delete[] m_samples;
}

void CSoundCardWriter::start()
{
	m_thread = std::thread(&CSoundCardWriter::entry, this);
}

void CSoundCardWriter::entry()
{
	wxLogMessage("Starting ALSA writer thread");

	while (!m_killed) {
		int nSamples = 2U * m_blockSize;
		m_callback->writeCallback(m_buffer, nSamples, m_id);

		if (nSamples == 0U) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		} else {
			if (m_channels == 1U) {
				for (int n = 0U; n < nSamples; n++)
					m_samples[n] = short(m_buffer[n] * 32767.0F);
			} else {
				int i = 0U;
				for (int n = 0U; n < nSamples; n++) {
					short sample = short(m_buffer[n] * 32767.0F);
					m_samples[i++] = sample;
					m_samples[i++] = sample;			// Same value to both channels
				}
			}

			int offset = 0U;
			snd_pcm_sframes_t ret;
			while ((ret = ::snd_pcm_writei(m_handle, m_samples + offset, nSamples - offset)) != (nSamples - offset)) {
				if (ret < 0) {
					if (ret != -EPIPE) {
						wxLogWarning("snd_pcm_writei returned %d (%s)", (int)ret, ::snd_strerror(ret));
					}

					::snd_pcm_recover(m_handle, ret, 1);
				} else {
					offset += ret;
				}
			}
		}
	}

	wxLogMessage("Stopping ALSA writer thread");

	::snd_pcm_close(m_handle);
}

void CSoundCardWriter::kill()
{
	m_killed = true;
}

void CSoundCardWriter::join()
{
	if (m_thread.joinable())
		m_thread.join();
}

bool CSoundCardWriter::isBusy() const
{
	snd_pcm_state_t state = ::snd_pcm_state(m_handle);

	return state == SND_PCM_STATE_RUNNING || state == SND_PCM_STATE_DRAINING;
}

#else

// PortAudio implementation for Windows and macOS

static int paCallback(const void* input, void* output, unsigned long frameCount,
                      const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData)
{
	CSoundCardReaderWriter* rw = static_cast<CSoundCardReaderWriter*>(userData);
	rw->callback(static_cast<const float*>(input), static_cast<float*>(output), frameCount);
	return paContinue;
}

CSoundCardReaderWriter::CSoundCardReaderWriter(const std::string& readDevice, const std::string& writeDevice, unsigned int sampleRate, unsigned int blockSize) :
m_readDevice(readDevice),
m_writeDevice(writeDevice),
m_sampleRate(sampleRate),
m_blockSize(blockSize),
m_callback(nullptr),
m_id(-1),
m_stream(nullptr)
{
}

CSoundCardReaderWriter::~CSoundCardReaderWriter()
{
}

void CSoundCardReaderWriter::setCallback(IAudioCallback* callback, int id)
{
	m_callback = callback;
	m_id = id;
}

bool CSoundCardReaderWriter::convertNameToDevices(PaDeviceIndex& inDev, PaDeviceIndex& outDev)
{
	inDev = paNoDevice;
	outDev = paNoDevice;

	int count = Pa_GetDeviceCount();
	for (int i = 0; i < count; i++) {
		const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
		if (info == nullptr)
			continue;

		std::string name(info->name);
		if (name == m_readDevice && info->maxInputChannels > 0)
			inDev = i;
		if (name == m_writeDevice && info->maxOutputChannels > 0)
			outDev = i;
	}

	return inDev != paNoDevice && outDev != paNoDevice;
}

bool CSoundCardReaderWriter::open()
{
	PaError err = Pa_Initialize();
	if (err != paNoError) {
		wxLogError("Pa_Initialize failed: %s", Pa_GetErrorText(err));
		return false;
	}

	PaDeviceIndex inDev, outDev;
	if (!convertNameToDevices(inDev, outDev)) {
		wxLogError("Could not find PortAudio devices: read='%s' write='%s'",
		           m_readDevice.c_str(), m_writeDevice.c_str());
		Pa_Terminate();
		return false;
	}

	PaStreamParameters inParams, outParams;
	inParams.device                    = inDev;
	inParams.channelCount              = 1;
	inParams.sampleFormat              = paFloat32;
	inParams.suggestedLatency          = Pa_GetDeviceInfo(inDev)->defaultLowInputLatency;
	inParams.hostApiSpecificStreamInfo = nullptr;

	outParams.device                    = outDev;
	outParams.channelCount              = 1;
	outParams.sampleFormat              = paFloat32;
	outParams.suggestedLatency          = Pa_GetDeviceInfo(outDev)->defaultLowOutputLatency;
	outParams.hostApiSpecificStreamInfo = nullptr;

	err = Pa_OpenStream(&m_stream, &inParams, &outParams, m_sampleRate, m_blockSize, paClipOff, paCallback, this);
	if (err != paNoError) {
		wxLogError("Pa_OpenStream failed: %s", Pa_GetErrorText(err));
		Pa_Terminate();
		return false;
	}

	err = Pa_StartStream(m_stream);
	if (err != paNoError) {
		wxLogError("Pa_StartStream failed: %s", Pa_GetErrorText(err));
		Pa_CloseStream(m_stream);
		m_stream = nullptr;
		Pa_Terminate();
		return false;
	}

	wxLogMessage("Opened PortAudio: read='%s' write='%s' rate=%u",
	             m_readDevice.c_str(), m_writeDevice.c_str(), m_sampleRate);

	return true;
}

void CSoundCardReaderWriter::close()
{
	if (m_stream != nullptr) {
		Pa_StopStream(m_stream);
		Pa_CloseStream(m_stream);
		m_stream = nullptr;
	}

	Pa_Terminate();
}

void CSoundCardReaderWriter::callback(const float* input, float* output, unsigned int nSamples)
{
	if (m_callback != nullptr) {
		m_callback->readCallback(input, nSamples, m_id);
		int n = static_cast<int>(nSamples);
		m_callback->writeCallback(output, n, m_id);
	}
}

std::vector<std::string> CSoundCardReaderWriter::getReadDevices()
{
	std::vector<std::string> devices;

	Pa_Initialize();

	int count = Pa_GetDeviceCount();
	for (int i = 0; i < count; i++) {
		const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
		if (info != nullptr && info->maxInputChannels > 0)
			devices.push_back(std::string(info->name));
	}

	Pa_Terminate();
	return devices;
}

std::vector<std::string> CSoundCardReaderWriter::getWriteDevices()
{
	std::vector<std::string> devices;

	Pa_Initialize();

	int count = Pa_GetDeviceCount();
	for (int i = 0; i < count; i++) {
		const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
		if (info != nullptr && info->maxOutputChannels > 0)
			devices.push_back(std::string(info->name));
	}

	Pa_Terminate();
	return devices;
}

#endif
