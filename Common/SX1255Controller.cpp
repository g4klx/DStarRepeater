/*
 *   Copyright (C) 2025 by Jonathan Naylor G4KLX and contributors
 *   SX1255 support: Karel Kadlec (OK1LBC) <xkk@post.cz>
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

#include "SX1255Controller.h"
#include "DStarDefines.h"
#include "Logger.h"

#include <alsa/asoundlib.h>
#include <linux/spi/spidev.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <thread>

// ---------------------------------------------------------------------------
// SX1255 RF constants
// ---------------------------------------------------------------------------

// RF I/Q sample rate exposed by the genericstereoaudiocodec overlay.
static const unsigned int RF_SR = 125000U;

// The SX1255 I/Q stream is two interleaved channels (I, Q).
static const unsigned int RF_CH = 2U;

// Decimation factor applied to the RX I/Q stream before FM demodulation.
static const unsigned int RX_DECIM = 5U;

// RX IF sample rate after decimation (125 kHz / 5).
static const unsigned int IF_SR = RF_SR / RX_DECIM;

// Number of taps in the RX decimation low-pass filter.
static const unsigned int IF_TAPS = 61U;

// ALSA period sizes (in frames).
static const unsigned int RX_PERIOD = 2048U;
static const unsigned int RX_BUF_PERIODS = 4U;
static const unsigned int TX_PERIOD = 1024U;
static const unsigned int TX_BUF_PERIODS = 4U;

// Baseband block size handed to the TX path (20 ms at 48 kHz).
static const unsigned int TX_BASE_BLOCK = DSTAR_RADIO_BLOCK_SIZE;

// Peak D-Star GMSK deviation for a unit-magnitude baseband sample (+-0.5
// represents the peak deviation).  h = 0.5 at 4800 baud => 1200 Hz.
static const float GMSK_PEAK_DEVIATION_HZ = 1200.0F;

// ---------------------------------------------------------------------------
// SX1255 SPI registers (see the reference sx1255-spi.c from the
// sx1255-fm-repeater project).
// ---------------------------------------------------------------------------

static const uint8_t REG_MODE       = 0x00U;
static const uint8_t REG_RX_FREQ_MSB = 0x01U;
static const uint8_t REG_RX_FREQ_MID = 0x02U;
static const uint8_t REG_RX_FREQ_LSB = 0x03U;
static const uint8_t REG_TX_FREQ_MSB = 0x04U;
static const uint8_t REG_TX_FREQ_MID = 0x05U;
static const uint8_t REG_TX_FREQ_LSB = 0x06U;
static const uint8_t REG_VERSION    = 0x07U;
static const uint8_t REG_TX_DAC     = 0x08U;
static const uint8_t REG_TX_PLL     = 0x0AU;
static const uint8_t REG_TX_DAC_TAPS = 0x0BU;
static const uint8_t REG_RX_GAIN    = 0x0CU;
static const uint8_t REG_RX_BW      = 0x0DU;
static const uint8_t REG_RX_PLL     = 0x0EU;
static const uint8_t REG_I2S_CFG    = 0x12U;
static const uint8_t REG_BITRATE    = 0x13U;

// Mode register bit definitions (bit 3 is the shared PLL/front-end enable).
static const uint8_t MODE_REF  = 0x01U;
static const uint8_t MODE_RX   = 0x0BU;   // REF + RX + enable
static const uint8_t MODE_TX   = 0x0DU;   // REF + TX + enable
static const uint8_t MODE_DUP  = 0x0FU;   // REF + RX + TX + enable
static const uint8_t MODE_OFF  = 0x00U;

static const float SX1255_CLK_HZ = 32.0e6F;

// ---------------------------------------------------------------------------
// DSP helper structures (complex samples and the RX/TX processing chains)
// ---------------------------------------------------------------------------

typedef struct { float re, im; } cf32;

typedef struct { cf32 x_prev, y_prev; float r; } dc_block_cc_t;

typedef struct { float phase, phase_inc; } rotator_t;

typedef struct { float phase, phase_inc; } fm_mod_t;

typedef struct {
	float taps[IF_TAPS];
	cf32  delay[IF_TAPS];
	int   idx;
} fir_decim_cc_t;

typedef struct { cf32 prev; bool has_prev; } nbfm_demod_t;

static void design_lowpass(float* taps, int ntaps, float fs, float fc)
{
	const float norm_cut = fc / fs;
	const int m = ntaps - 1;
	float sum = 0.0F;

	for (int n = 0; n < ntaps; n++) {
		float x = (float)n - (float)m / 2.0F;
		float h = fabsf(x) < 1e-6F
			? 2.0F * norm_cut
			: sinf(2.0F * (float)M_PI * norm_cut * x) / ((float)M_PI * x);
		float w = 0.54F - 0.46F * cosf(2.0F * (float)M_PI * n / (float)m);
		taps[n] = h * w;
		sum += taps[n];
	}

	for (int n = 0; n < ntaps; n++)
		taps[n] /= sum;
}

static void dc_block_cc_init(dc_block_cc_t* d, float r)
{
	::memset(d, 0, sizeof(*d));
	d->r = r;
}

static inline cf32 dc_block_cc_process(dc_block_cc_t* d, cf32 x)
{
	cf32 y;
	y.re = x.re - d->x_prev.re + d->r * d->y_prev.re;
	y.im = x.im - d->x_prev.im + d->r * d->y_prev.im;
	d->x_prev = x;
	d->y_prev = y;
	return y;
}

static void rotator_init(rotator_t* r, float fs, float shiftHz)
{
	r->phase = 0.0F;
	r->phase_inc = 2.0F * (float)M_PI * shiftHz / fs;
}

static inline cf32 rotator_process(rotator_t* r, cf32 x)
{
	float c = cosf(r->phase);
	float s = sinf(r->phase);
	cf32 y;
	y.re = x.re * c - x.im * s;
	y.im = x.re * s + x.im * c;
	r->phase += r->phase_inc;
	if (r->phase > (float)M_PI)
		r->phase -= 2.0F * (float)M_PI;
	else if (r->phase < -(float)M_PI)
		r->phase += 2.0F * (float)M_PI;
	return y;
}

static void fir_decim_cc_init(fir_decim_cc_t* f, float fs, float fc)
{
	::memset(f, 0, sizeof(*f));
	design_lowpass(f->taps, (int)IF_TAPS, fs, fc);
}

static inline void fir_decim_cc_push(fir_decim_cc_t* f, cf32 x)
{
	f->delay[f->idx++] = x;
	if (f->idx >= (int)IF_TAPS)
		f->idx = 0;
}

static inline cf32 fir_decim_cc_compute(const fir_decim_cc_t* f)
{
	cf32 y = {0.0F, 0.0F};
	int idx = f->idx;
	for (int n = 0; n < (int)IF_TAPS; n++) {
		if (--idx < 0)
			idx = (int)IF_TAPS - 1;
		y.re += f->taps[n] * f->delay[idx].re;
		y.im += f->taps[n] * f->delay[idx].im;
	}
	return y;
}

static void nbfm_demod_init(nbfm_demod_t* d)
{
	::memset(d, 0, sizeof(*d));
}

static inline float nbfm_demod(nbfm_demod_t* d, cf32 x)
{
	if (!d->has_prev) {
		d->prev = x;
		d->has_prev = true;
		return 0.0F;
	}
	float re = x.re * d->prev.re + x.im * d->prev.im;
	float im = x.im * d->prev.re - x.re * d->prev.im;
	d->prev = x;
	return atan2f(im, re);
}

static void fm_mod_init(fm_mod_t* m, float sensitivity)
{
	m->phase = 0.0F;
	m->phase_inc = sensitivity;
}

static inline cf32 fm_mod_process(fm_mod_t* m, float x)
{
	m->phase += m->phase_inc * x;
	if (m->phase > (float)M_PI)
		m->phase -= 2.0F * (float)M_PI;
	else if (m->phase < -(float)M_PI)
		m->phase += 2.0F * (float)M_PI;
	cf32 y;
	y.re = cosf(m->phase);
	y.im = sinf(m->phase);
	return y;
}

static inline float clampf(float x, float lo, float hi)
{
	if (x < lo) return lo;
	if (x > hi) return hi;
	return x;
}

static inline int32_t f_to_s32(float x)
{
	x = clampf(x, -1.0F, 1.0F);
	return (int32_t)lrintf(x * 2147483000.0F);
}

// ---------------------------------------------------------------------------
// libgpiod (v2) support
//
// libgpiod is loaded dynamically so the build does not require the libgpiod
// development headers and the binary still runs on systems without libgpiod.
// It is used both for the SX1255 hardware reset and for the optional PA PTT
// output.  The legacy sysfs GPIO path remains available as a reset fallback.
// ---------------------------------------------------------------------------

struct gpiod_chip;
struct gpiod_line_settings;
struct gpiod_line_config;
struct gpiod_request_config;
struct gpiod_line_request;

typedef gpiod_chip*           (*gpiod_chip_open_fn)(const char*);
typedef void                  (*gpiod_chip_close_fn)(gpiod_chip*);
typedef gpiod_line_settings*  (*gpiod_settings_new_fn)();
typedef void                  (*gpiod_settings_free_fn)(gpiod_line_settings*);
typedef int                   (*gpiod_settings_set_direction_fn)(gpiod_line_settings*, int);
typedef gpiod_line_config*    (*gpiod_config_new_fn)();
typedef void                  (*gpiod_config_free_fn)(gpiod_line_config*);
typedef int                   (*gpiod_config_add_settings_fn)(gpiod_line_config*, const unsigned int*, size_t, gpiod_line_settings*);
typedef gpiod_request_config* (*gpiod_req_new_fn)();
typedef void                  (*gpiod_req_free_fn)(gpiod_request_config*);
typedef void                  (*gpiod_req_set_consumer_fn)(gpiod_request_config*, const char*);
typedef gpiod_line_request*   (*gpiod_request_lines_fn)(gpiod_chip*, gpiod_request_config*, gpiod_line_config*);
typedef void                  (*gpiod_release_fn)(gpiod_line_request*);
typedef int                   (*gpiod_set_value_fn)(gpiod_line_request*, unsigned int, int);

struct GpiodApi {
	bool attempted = false;
	bool loaded    = false;
	void* handle   = nullptr;

	gpiod_chip_open_fn            chip_open           = nullptr;
	gpiod_chip_close_fn           chip_close          = nullptr;
	gpiod_settings_new_fn         settings_new        = nullptr;
	gpiod_settings_free_fn        settings_free       = nullptr;
	gpiod_settings_set_direction_fn settings_set_direction = nullptr;
	gpiod_config_new_fn           config_new          = nullptr;
	gpiod_config_free_fn          config_free         = nullptr;
	gpiod_config_add_settings_fn  config_add_settings = nullptr;
	gpiod_req_new_fn              req_new             = nullptr;
	gpiod_req_free_fn             req_free            = nullptr;
	gpiod_req_set_consumer_fn     req_set_consumer    = nullptr;
	gpiod_request_lines_fn        request_lines       = nullptr;
	gpiod_release_fn              release             = nullptr;
	gpiod_set_value_fn            set_value           = nullptr;

	bool load()
	{
		if (loaded)
			return true;
		if (attempted)
			return false;
		attempted = true;

		handle = ::dlopen("libgpiod.so.3", RTLD_NOW | RTLD_LOCAL);
		if (handle == nullptr) {
			wxLogWarning("SX1255: cannot load libgpiod.so.3 (%s)", ::dlerror());
			return false;
		}

		chip_open           = (gpiod_chip_open_fn)::dlsym(handle, "gpiod_chip_open");
		chip_close          = (gpiod_chip_close_fn)::dlsym(handle, "gpiod_chip_close");
		settings_new        = (gpiod_settings_new_fn)::dlsym(handle, "gpiod_line_settings_new");
		settings_free       = (gpiod_settings_free_fn)::dlsym(handle, "gpiod_line_settings_free");
		settings_set_direction = (gpiod_settings_set_direction_fn)::dlsym(handle, "gpiod_line_settings_set_direction");
		config_new          = (gpiod_config_new_fn)::dlsym(handle, "gpiod_line_config_new");
		config_free         = (gpiod_config_free_fn)::dlsym(handle, "gpiod_line_config_free");
		config_add_settings = (gpiod_config_add_settings_fn)::dlsym(handle, "gpiod_line_config_add_line_settings");
		req_new             = (gpiod_req_new_fn)::dlsym(handle, "gpiod_request_config_new");
		req_free            = (gpiod_req_free_fn)::dlsym(handle, "gpiod_request_config_free");
		req_set_consumer    = (gpiod_req_set_consumer_fn)::dlsym(handle, "gpiod_request_config_set_consumer");
		request_lines       = (gpiod_request_lines_fn)::dlsym(handle, "gpiod_chip_request_lines");
		release             = (gpiod_release_fn)::dlsym(handle, "gpiod_line_request_release");
		set_value           = (gpiod_set_value_fn)::dlsym(handle, "gpiod_line_request_set_value");

		if (chip_open == nullptr || chip_close == nullptr ||
		    settings_new == nullptr || settings_free == nullptr ||
		    settings_set_direction == nullptr ||
		    config_new == nullptr || config_free == nullptr ||
		    config_add_settings == nullptr ||
		    req_new == nullptr || req_free == nullptr ||
		    request_lines == nullptr || release == nullptr ||
		    set_value == nullptr) {
			wxLogWarning("SX1255: libgpiod.so.3 is missing required symbols");
			::dlclose(handle);
			handle = nullptr;
			return false;
		}

		loaded = true;
		return true;
	}
};

static GpiodApi g_gpiod;

static bool gpiodRequestOutput(const std::string& chip, unsigned int line, const char* consumer,
                               gpiod_chip*& outChip, gpiod_line_request*& outRequest)
{
	outChip = nullptr;
	outRequest = nullptr;

	if (!g_gpiod.load())
		return false;

	gpiod_chip* c = g_gpiod.chip_open(chip.c_str());
	if (c == nullptr) {
		int savedErrno = errno;
		wxLogWarning("SX1255: cannot open %s (%s)", chip.c_str(), ::strerror(savedErrno));
		return false;
	}

	gpiod_line_settings*  ls = g_gpiod.settings_new();
	gpiod_line_config*    lc = g_gpiod.config_new();
	gpiod_request_config* rc = g_gpiod.req_new();
	gpiod_line_request*   req = nullptr;

	if (ls != nullptr && lc != nullptr && rc != nullptr) {
		if (g_gpiod.req_set_consumer != nullptr)
			g_gpiod.req_set_consumer(rc, consumer);

		unsigned int offsets[1] = { line };

		// 3 = GPIOD_LINE_DIRECTION_OUTPUT (libgpiod v2 enum).
		if (g_gpiod.settings_set_direction(ls, 3) == 0 &&
		    g_gpiod.config_add_settings(lc, offsets, 1U, ls) == 0)
			req = g_gpiod.request_lines(c, rc, lc);
	}

	if (req == nullptr) {
		int savedErrno = errno;
		wxLogWarning("SX1255: cannot request %s line %u as output (%s)",
		             chip.c_str(), line, ::strerror(savedErrno));
	}

	if (ls != nullptr)
		g_gpiod.settings_free(ls);
	if (lc != nullptr)
		g_gpiod.config_free(lc);
	if (rc != nullptr)
		g_gpiod.req_free(rc);

	if (req == nullptr) {
		g_gpiod.chip_close(c);
		return false;
	}

	outChip = c;
	outRequest = req;
	return true;
}

static bool gpiodReset(const std::string& chip, unsigned int line)
{
	gpiod_chip* c = nullptr;
	gpiod_line_request* req = nullptr;
	if (!gpiodRequestOutput(chip, line, "dstarrepeaterd", c, req))
		return false;

	// Replicate the reference reset sequence from sx1255-spi.c:
	// drive the pin high, then low.
	g_gpiod.set_value(req, line, 1);
	::usleep(100000U);
	g_gpiod.set_value(req, line, 0);
	::usleep(250000U);
	g_gpiod.release(req);
	g_gpiod.chip_close(c);
	return true;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CSX1255Controller::CSX1255Controller(const std::string& rxDevice, const std::string& txDevice,
                                     const std::string& spiDevice,
                                     unsigned int rxFrequency, unsigned int txFrequency,
                                     float rxShiftHz, float txDeviationHz,
                                     bool rxInvert, bool txInvert,
                                     float rxLevel, float txLevel,
                                     unsigned int txDelay, unsigned int txTail,
                                     unsigned int spiSpeed, int resetGpio,
                                     const std::string& resetChip, unsigned int resetLine,
                                     const std::string& pttChip, unsigned int pttLine, bool pttInvert) :
CSoundCardController(rxDevice, txDevice, rxInvert, txInvert, rxLevel, txLevel, txDelay, txTail),
m_rxDevice(rxDevice),
m_txDevice(txDevice),
m_spiDevice(spiDevice),
m_rxFrequency(rxFrequency),
m_txFrequency(txFrequency),
m_rxShiftHz(rxShiftHz),
m_txDeviationHz(txDeviationHz > 0.0F ? txDeviationHz : GMSK_PEAK_DEVIATION_HZ),
m_spiSpeed(spiSpeed),
m_resetGpio(resetGpio),
m_resetChip(resetChip),
m_resetLine(resetLine),
m_pttChip(pttChip),
m_pttLine(pttLine),
m_pttInvert(pttInvert),
m_spiFd(-1),
m_pcmRx(nullptr),
m_pcmTx(nullptr),
m_rxThread(),
m_txThread(),
m_txEnabled(false),
m_txActive(false),
m_pttChipHandle(nullptr),
m_pttRequest(nullptr),
m_pttAsserted(false)
{
}

CSX1255Controller::~CSX1255Controller()
{
}

// ---------------------------------------------------------------------------
// SPI control
// ---------------------------------------------------------------------------

bool CSX1255Controller::spiOpen()
{
	if (m_spiFd >= 0)
		return true;

	int fd = ::open(m_spiDevice.c_str(), O_RDWR);
	if (fd < 0) {
		wxLogError("SX1255: cannot open SPI device %s (%s)", m_spiDevice.c_str(), ::strerror(errno));
		return false;
	}

	uint8_t mode = 0U;
	uint8_t bits = 8U;
	if (::ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
	    ::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
	    ::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &m_spiSpeed) < 0) {
		wxLogError("SX1255: cannot configure SPI device %s (%s)", m_spiDevice.c_str(), ::strerror(errno));
		::close(fd);
		return false;
	}

	m_spiFd = fd;
	return true;
}

void CSX1255Controller::spiClose()
{
	if (m_spiFd >= 0) {
		::close(m_spiFd);
		m_spiFd = -1;
	}
}

bool CSX1255Controller::spiReset()
{
	// Preferred path: reset through libgpiod using an explicit gpiochip and
	// line offset.  This is the reliable interface on Raspberry Pi 5 where the
	// legacy sysfs GPIO numbers differ from the classic BCM2835 numbering.
	if (!m_resetChip.empty()) {
		if (gpiodReset(m_resetChip, m_resetLine)) {
			wxLogInfo("SX1255: reset via libgpiod on %s line %u", m_resetChip.c_str(), m_resetLine);
			return true;
		}
		wxLogWarning("SX1255: libgpiod reset on %s line %u failed, falling back to sysfs", m_resetChip.c_str(), m_resetLine);
	}

	// Legacy fallback: sysfs GPIO export by global GPIO number.
	if (m_resetGpio <= 0) {
		wxLogWarning("SX1255: no reset pin configured (set ResetChip/ResetLine or ResetGPIO)");
		return true;
	}

	char path[128];
	::snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", m_resetGpio);

	if (::access(path, W_OK) != 0) {
		// The pin has not been exported yet; try the legacy sysfs export API.
		FILE* fp = ::fopen("/sys/class/gpio/export", "wb");
		if (fp != nullptr) {
			::fprintf(fp, "%d", m_resetGpio);
			::fclose(fp);
			::usleep(250000U);
		}

		::snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", m_resetGpio);
		fp = ::fopen(path, "wb");
		if (fp != nullptr) {
			::fwrite("out", 1U, 3U, fp);
			::fclose(fp);
		}
	}

	::snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", m_resetGpio);
	if (::access(path, W_OK) != 0) {
		wxLogWarning("SX1255: reset GPIO %d is not available, assuming the chip is already initialised", m_resetGpio);
		return true;
	}

	FILE* fp = ::fopen(path, "wb");
	if (fp == nullptr)
		return true;
	::fwrite("1", 1U, 1U, fp);
	::fclose(fp);
	::usleep(100000U);
	fp = ::fopen(path, "wb");
	if (fp != nullptr) {
		::fwrite("0", 1U, 1U, fp);
		::fclose(fp);
	}
	::usleep(250000U);
	return true;
}

bool CSX1255Controller::spiSetFrequency(bool tx, unsigned int freqHz)
{
	if (freqHz < 410000000U || freqHz > 510000000U) {
		wxLogError("SX1255: frequency %u Hz is outside the 410-510 MHz range", freqHz);
		return false;
	}

	uint32_t val = (uint32_t)lroundf((float)freqHz * 1048576.0F / SX1255_CLK_HZ);
	uint8_t msb = (uint8_t)((val >> 16) & 0xFFU);
	uint8_t mid = (uint8_t)((val >> 8) & 0xFFU);
	uint8_t lsb = (uint8_t)(val & 0xFFU);

	uint8_t regMsb = tx ? REG_TX_FREQ_MSB : REG_RX_FREQ_MSB;
	uint8_t regMid = tx ? REG_TX_FREQ_MID : REG_RX_FREQ_MID;
	uint8_t regLsb = tx ? REG_TX_FREQ_LSB : REG_RX_FREQ_LSB;

	uint8_t txb[2];
	uint8_t rxb[2] = {0U, 0U};
	struct spi_ioc_transfer tr;
	::memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)txb;
	tr.rx_buf = (unsigned long)rxb;
	tr.len = 2U;
	tr.speed_hz = m_spiSpeed;
	tr.bits_per_word = 8U;

	txb[0] = (uint8_t)(0x80U | regMsb); txb[1] = msb;
	if (::ioctl(m_spiFd, SPI_IOC_MESSAGE(1), &tr) < 0) return false;
	txb[0] = (uint8_t)(0x80U | regMid); txb[1] = mid;
	if (::ioctl(m_spiFd, SPI_IOC_MESSAGE(1), &tr) < 0) return false;
	txb[0] = (uint8_t)(0x80U | regLsb); txb[1] = lsb;
	if (::ioctl(m_spiFd, SPI_IOC_MESSAGE(1), &tr) < 0) return false;

	return true;
}

bool CSX1255Controller::spiSetMode(unsigned char mode)
{
	uint8_t txb[2] = { (uint8_t)(0x80U | REG_MODE), mode };
	uint8_t rxb[2] = {0U, 0U};
	struct spi_ioc_transfer tr;
	::memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)txb;
	tr.rx_buf = (unsigned long)rxb;
	tr.len = 2U;
	tr.speed_hz = m_spiSpeed;
	tr.bits_per_word = 8U;

	if (::ioctl(m_spiFd, SPI_IOC_MESSAGE(1), &tr) < 0) {
		wxLogError("SX1255: cannot write mode register (%s)", ::strerror(errno));
		return false;
	}
	return true;
}

bool CSX1255Controller::spiInitChip()
{
	uint8_t txb[2];
	uint8_t rxb[2] = {0U, 0U};
	struct spi_ioc_transfer tr;
	::memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)txb;
	tr.rx_buf = (unsigned long)rxb;
	tr.len = 2U;
	tr.speed_hz = m_spiSpeed;
	tr.bits_per_word = 8U;

	// Verify the silicon version register.
	txb[0] = REG_VERSION; txb[1] = 0U;
	if (::ioctl(m_spiFd, SPI_IOC_MESSAGE(1), &tr) < 0) {
		wxLogError("SX1255: cannot read version register (%s)", ::strerror(errno));
		return false;
	}
	if (rxb[1] != 0x11U) {
		wxLogWarning("SX1255: unexpected version 0x%02X (expected 0x11)", rxb[1]);
	}

	struct { uint8_t reg; uint8_t val; } initRegs[] = {
		{ REG_BITRATE,    (uint8_t)((0U << 7) | (0U << 6) | (5U << 3) | (1U << 2)) }, // 0x2C
		{ REG_I2S_CFG,    (uint8_t)((2U << 4) | 2U) },                               // 0x22
		{ REG_RX_BW,      (uint8_t)((0x01U << 5) | (0x05U << 2) | 0x03U) },          // 0x37
		{ REG_RX_PLL,     0x00U },
		{ REG_RX_GAIN,    (uint8_t)((0x01U << 5) | (0x0FU << 1) | 0x00U) },          // 0x3E
		{ REG_TX_DAC_TAPS, 5U },
		{ REG_TX_PLL,     0x00U },
		{ REG_TX_DAC,     (uint8_t)((1U << 4) | 0x0EU) },                            // 0x1E
	};

	for (unsigned int i = 0U; i < sizeof(initRegs) / sizeof(initRegs[0]); i++) {
		txb[0] = (uint8_t)(0x80U | initRegs[i].reg);
		txb[1] = initRegs[i].val;
		if (::ioctl(m_spiFd, SPI_IOC_MESSAGE(1), &tr) < 0) {
			wxLogError("SX1255: cannot initialise register 0x%02X (%s)", initRegs[i].reg, ::strerror(errno));
			return false;
		}
		::usleep(10000U);
	}

	return true;
}

bool CSX1255Controller::pttOpen()
{
	// An empty chip means the PTT output is not used.
	if (m_pttChip.empty())
		return true;

	gpiod_chip* c = nullptr;
	gpiod_line_request* req = nullptr;
	if (!gpiodRequestOutput(m_pttChip, m_pttLine, "dstarrepeaterd-ptt", c, req))
		return false;

	m_pttChipHandle = c;
	m_pttRequest    = req;
	pttSet(false);
	wxLogInfo("SX1255: PA PTT on %s line %u (%s)", m_pttChip.c_str(), m_pttLine,
	          m_pttInvert ? "active low" : "active high");
	return true;
}

void CSX1255Controller::pttClose()
{
	if (m_pttRequest != nullptr && g_gpiod.loaded) {
		pttSet(false);
		g_gpiod.release((gpiod_line_request*)m_pttRequest);
		m_pttRequest = nullptr;
	}
	if (m_pttChipHandle != nullptr && g_gpiod.loaded) {
		g_gpiod.chip_close((gpiod_chip*)m_pttChipHandle);
		m_pttChipHandle = nullptr;
	}
	m_pttAsserted = false;
}

void CSX1255Controller::pttSet(bool active)
{
	if (m_pttRequest == nullptr || !g_gpiod.loaded)
		return;

	int value = active ? (m_pttInvert ? 0 : 1) : (m_pttInvert ? 1 : 0);
	g_gpiod.set_value((gpiod_line_request*)m_pttRequest, m_pttLine, value);
}

// ---------------------------------------------------------------------------
// ALSA I/Q transport
// ---------------------------------------------------------------------------

static snd_pcm_t* openPcm(const std::string& device, snd_pcm_stream_t stream,
                          unsigned int period, unsigned int bufPeriods)
{
	snd_pcm_t* pcm = nullptr;
	if (::snd_pcm_open(&pcm, device.c_str(), stream, SND_PCM_NONBLOCK) < 0)
		return nullptr;

	snd_pcm_hw_params_t* hw = nullptr;
	snd_pcm_hw_params_alloca(&hw);
	::snd_pcm_hw_params_any(pcm, hw);
	::snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	::snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S32_LE);
	::snd_pcm_hw_params_set_channels(pcm, hw, RF_CH);

	unsigned int rate = RF_SR;
	::snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, nullptr);

	snd_pcm_uframes_t p = (snd_pcm_uframes_t)period;
	::snd_pcm_hw_params_set_period_size_near(pcm, hw, &p, nullptr);
	snd_pcm_uframes_t b = p * (snd_pcm_uframes_t)bufPeriods;
	::snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &b);

	if (::snd_pcm_hw_params(pcm, hw) < 0) {
		::snd_pcm_close(pcm);
		return nullptr;
	}

	::snd_pcm_prepare(pcm);
	return pcm;
}

bool CSX1255Controller::alsaOpen()
{
	m_pcmRx = openPcm(m_rxDevice, SND_PCM_STREAM_CAPTURE, RX_PERIOD, RX_BUF_PERIODS);
	if (m_pcmRx == nullptr) {
		wxLogError("SX1255: cannot open RX I/Q capture device %s", m_rxDevice.c_str());
		return false;
	}

	m_pcmTx = openPcm(m_txDevice, SND_PCM_STREAM_PLAYBACK, TX_PERIOD, TX_BUF_PERIODS);
	if (m_pcmTx == nullptr) {
		wxLogError("SX1255: cannot open TX I/Q playback device %s", m_txDevice.c_str());
		::snd_pcm_close(m_pcmRx);
		m_pcmRx = nullptr;
		return false;
	}

	return true;
}

void CSX1255Controller::alsaClose()
{
	if (m_pcmRx != nullptr) {
		::snd_pcm_close(m_pcmRx);
		m_pcmRx = nullptr;
	}
	if (m_pcmTx != nullptr) {
		::snd_pcm_close(m_pcmTx);
		m_pcmTx = nullptr;
	}
}

static snd_pcm_sframes_t readFrames(snd_pcm_t* pcm, int32_t* buf, snd_pcm_uframes_t frames)
{
	for (;;) {
		snd_pcm_sframes_t r = ::snd_pcm_readi(pcm, buf, frames);
		if (r == -EAGAIN)
			return 0;
		if (r == -EPIPE) {
			::snd_pcm_prepare(pcm);
			continue;
		}
		if (r < 0)
			return r;
		return r;
	}
}

static int writeFrames(snd_pcm_t* pcm, const int32_t* buf, snd_pcm_uframes_t frames)
{
	const int32_t* p = buf;
	snd_pcm_uframes_t left = frames;
	while (left > 0U) {
		snd_pcm_sframes_t w = ::snd_pcm_writei(pcm, p, left);
		if (w == -EAGAIN) {
			::usleep(1000U);
			continue;
		}
		if (w == -EPIPE) {
			::snd_pcm_prepare(pcm);
			continue;
		}
		if (w < 0)
			return (int)w;
		left -= (snd_pcm_uframes_t)w;
		p += (size_t)w * RF_CH;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------

bool CSX1255Controller::start()
{
	if (!spiOpen())
		return false;

	spiReset();

	if (!spiInitChip()) {
		spiClose();
		return false;
	}

	// Tune the RX synthesiser low by RXShift so the wanted carrier ends up at
	// DC after the software mixer, avoiding the LO DC spike.
	int rxLo = (int)m_rxFrequency + (int)m_rxShiftHz;
	if (!spiSetFrequency(false, (unsigned int)rxLo)) {
		spiClose();
		return false;
	}
	if (!spiSetFrequency(true, m_txFrequency)) {
		spiClose();
		return false;
	}

	// Start receive-only: RX front-end on, TX front-end off.
	if (!spiSetMode(MODE_RX)) {
		spiClose();
		return false;
	}
	m_txEnabled = false;

	if (!alsaOpen()) {
		spiSetMode(MODE_OFF);
		spiClose();
		return false;
	}

	// Optional PA PTT output.  Failure is non-fatal: the modem still works,
	// but the external PA is not keyed.
	if (!pttOpen())
		wxLogError("SX1255: failed to open PA PTT output");

	// Demodulator thread (reuses CSoundCardController::entry()).
	m_thread = std::thread([this]() { entry(); });
	m_rxThread = std::thread(&CSX1255Controller::rxThread, this);
	m_txThread = std::thread(&CSX1255Controller::txThread, this);

	wxLogInfo("SX1255 modem started: RX=%u Hz (LO=%d Hz), TX=%u Hz, shift=%.1f Hz",
	          m_rxFrequency, rxLo, m_txFrequency, m_rxShiftHz);
	return true;
}

void CSX1255Controller::stop()
{
	m_stopped = true;

	if (m_pcmRx != nullptr)
		::snd_pcm_abort(m_pcmRx);
	if (m_pcmTx != nullptr)
		::snd_pcm_abort(m_pcmTx);

	if (m_rxThread.joinable())
		m_rxThread.join();
	if (m_txThread.joinable())
		m_txThread.join();
	if (m_thread.joinable())
		m_thread.join();

	alsaClose();

	pttClose();

	if (m_spiFd >= 0)
		spiSetMode(MODE_OFF);
	spiClose();

	wxLogInfo("SX1255 modem stopped");
}

bool CSX1255Controller::isTX()
{
	return m_txActive.load();
}

// ---------------------------------------------------------------------------
// RX thread: SX1255 I/Q -> D-Star baseband
// ---------------------------------------------------------------------------

void CSX1255Controller::rxThread()
{
	const unsigned int block = RX_PERIOD;

	int32_t* iq = new int32_t[(size_t)block * RF_CH];
	float*   out = new float[block];

	dc_block_cc_t dc;
	rotator_t     rot;
	fir_decim_cc_t lpf;
	nbfm_demod_t  demod;

	dc_block_cc_init(&dc, 0.999F);
	rotator_init(&rot, (float)RF_SR, m_rxShiftHz);
	fir_decim_cc_init(&lpf, (float)RF_SR, 7500.0F);
	nbfm_demod_init(&demod);

	// Scale the discriminator output so +-GMSK_PEAK_DEVIATION_HZ maps to +-1.0.
	float demodScale = (float)IF_SR / (2.0F * (float)M_PI * GMSK_PEAK_DEVIATION_HZ);

	double resampStep = (double)IF_SR / (double)DSTAR_RADIO_SAMPLE_RATE;
	double nextOutT = 0.0;
	double inT = 0.0;
	float  prevIf = 0.0F;
	bool   havePrev = false;
	unsigned int outPos = 0U;
	int decPhase = 0;

	wxLogInfo("SX1255 RX thread started");

	while (!m_stopped) {
		snd_pcm_sframes_t got = readFrames(m_pcmRx, iq, (snd_pcm_uframes_t)block);
		if (got == 0) {
			::usleep(1000U);
			continue;
		}
		if (got < 0)
			break;

		for (snd_pcm_sframes_t i = 0; i < got; i++) {
			cf32 x;
			x.re = (float)iq[i * RF_CH + 0U] / 2147483648.0F;
			x.im = (float)iq[i * RF_CH + 1U] / 2147483648.0F;

			x = dc_block_cc_process(&dc, x);
			x = rotator_process(&rot, x);

			fir_decim_cc_push(&lpf, x);
			if (++decPhase < (int)RX_DECIM)
				continue;
			decPhase = 0;

			cf32 xif = fir_decim_cc_compute(&lpf);
			float af = nbfm_demod(&demod, xif) * demodScale;

			if (!havePrev) {
				prevIf = af;
				havePrev = true;
				continue;
			}

			inT += 1.0;
			while (nextOutT <= inT) {
				double frac = nextOutT - (inT - 1.0);
				float a = prevIf + (float)frac * (af - prevIf);
				out[outPos++] = a;
				if (outPos >= block) {
					m_rxAudio.addData(out, outPos);
					outPos = 0U;
				}
				nextOutT += resampStep;
			}
			prevIf = af;
		}
	}

	if (outPos > 0U)
		m_rxAudio.addData(out, outPos);

	delete[] iq;
	delete[] out;

	wxLogInfo("SX1255 RX thread stopped");
}

// ---------------------------------------------------------------------------
// TX thread: D-Star baseband -> SX1255 I/Q
// ---------------------------------------------------------------------------

void CSX1255Controller::txThread()
{
	float* base = new float[TX_BASE_BLOCK];

	// Sample-and-hold upsample from 48 kHz to 125 kHz, FM-modulating directly
	// at the RF rate.  sensitivity maps the +-0.5 GMSK baseband to +-peak
	// deviation.
	float sensitivity = 2.0F * (float)M_PI * (m_txDeviationHz / 0.5F) / (float)RF_SR;
	fm_mod_t mod;
	fm_mod_init(&mod, sensitivity);

	const unsigned int maxRf = (TX_BASE_BLOCK * RF_SR + DSTAR_RADIO_SAMPLE_RATE - 1U) / DSTAR_RADIO_SAMPLE_RATE + 8U;
	int32_t* iq = new int32_t[(size_t)maxRf * RF_CH];
	int32_t* silence = new int32_t[(size_t)TX_PERIOD * RF_CH];
	::memset(silence, 0, sizeof(int32_t) * (size_t)TX_PERIOD * RF_CH);

	typedef std::chrono::steady_clock clock;
	clock::time_point lastData = clock::now();
	const int hangMs = 50;

	wxLogInfo("SX1255 TX thread started");

	while (!m_stopped) {
		unsigned int n = m_txAudio.getData(base, TX_BASE_BLOCK);

		if (n == 0U) {
			if (m_txEnabled.load()) {
				int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - lastData).count();
				if (elapsed >= hangMs) {
					spiSetMode(MODE_RX);
					m_txEnabled = false;
					if (m_pttAsserted) {
						pttSet(false);
						m_pttAsserted = false;
					}
					m_txActive = false;
				}
			}
			writeFrames(m_pcmTx, silence, TX_PERIOD);
			continue;
		}

		m_txActive = true;
		lastData = clock::now();
		if (!m_txEnabled.load()) {
			// Key the PA before enabling the SX1255 TX front-end so the
			// external amplifier is already up when RF appears.
			if (!m_pttAsserted) {
				pttSet(true);
				m_pttAsserted = true;
			}
			spiSetMode(MODE_DUP);
			m_txEnabled = true;
		}

		int rfFrames = 0;
		int upAcc = 0;
		for (unsigned int i = 0U; i < n; i++) {
			float x = clampf(base[i], -1.0F, 1.0F);
			upAcc += (int)RF_SR;
			while (upAcc >= (int)DSTAR_RADIO_SAMPLE_RATE && rfFrames < (int)maxRf) {
				upAcc -= (int)DSTAR_RADIO_SAMPLE_RATE;
				cf32 s = fm_mod_process(&mod, x);
				iq[(size_t)rfFrames * RF_CH + 0U] = f_to_s32(s.re);
				iq[(size_t)rfFrames * RF_CH + 1U] = f_to_s32(s.im);
				rfFrames++;
			}
		}

		if (rfFrames > 0)
			writeFrames(m_pcmTx, iq, (snd_pcm_uframes_t)rfFrames);
	}

	if (m_txEnabled.load()) {
		spiSetMode(MODE_RX);
		m_txEnabled = false;
	}
	if (m_pttAsserted) {
		pttSet(false);
		m_pttAsserted = false;
	}
	m_txActive = false;

	delete[] base;
	delete[] iq;
	delete[] silence;

	wxLogInfo("SX1255 TX thread stopped");
}
