# Instalace na Raspberry Pi Zero 2 W

> Autor SX1255 podpory: Karel Kadlec (OK1LBC) <xkk@post.cz>

Tento návod popisuje nasazení `dstarrepeaterd` s SX1255 HATem na vzdálený
Raspberry Pi Zero 2 W. Počítá s tím, že hardware (SX1255) a potřebný kernel
s audio overlayem jsou už připravené — stejně jako pro projekt
`sx1255-fm-repeater`.

## 1. Předpoklady: kernel a overlay

SX1255 přes I2S potřebuje speciální audio codec overlay. Nainstaluj na zařízení:

```sh
# 1. kernel z balíčku projektu sx1255-fm-repeater
sudo apt install ./rpi-zero2-kernel-6.18.36-myzero2+.deb

# 2. device-tree overlay
sudo install -m 0644 genericstereoaudiocodec.dtbo \
  /boot/firmware/overlays/genericstereoaudiocodec.dtbo
```

V `/boot/firmware/config.txt` (na starším OS `/boot/config.txt`) povol SPI,
I2S a overlay:

```ini
dtparam=spi=on
dtparam=i2s=on
dtoverlay=genericstereoaudiocodec
```

Reboot a ověř, že ALSA vidí kartu `GenericStereoAu`:

```sh
sudo reboot
aplay -l
arecord -l
```

## 2. Přenos zdrojáků na zařízení

Na Pi Zero 2 W (přes `ssh`) buď naklonuj repozitář, nebo zkopíruj lokální
adresář:

```sh
# varianta A: git
ssh pi@zero2w 'git clone --depth 1 https://github.com/g4klx/DStarRepeater.git'

# varianta B: rsync/scp z tvého počítače
rsync -av --exclude 'DStarRepeater/DStarRepeater/*.o' \
  /home/xkk/projects/repeater/DStarRepeater/ pi@zero2w:DStarRepeater/
```

## 3. Závislosti

```sh
sudo apt-get update
sudo apt-get install -y g++ make libasound2-dev libusb-1.0-0-dev
```

SX1255 driver potřebuje jen ALSA a SPI `ioctl`. `libgpiod3` (runtime) se
používá na reset pinu; vývojové hlavičky nejsou potřeba, protože se načítá
dynamicky. Pro MQTT/GPIO bys potřeboval `libmosquitto-dev` a `wiringpi` —
skript je proto staví s `MQTT=0 GPIO=0`.

## 4. Build a instalace

Nejjednodušší je spustit připravený instalační skript přímo na zařízení:

```sh
cd DStarRepeater
sudo ./deploy/install-zero2w.sh
```

Skript:

- přeloží `dstarrepeaterd` (`MQTT=0 GPIO=0`),
- nainstaluje binárku do `/usr/bin` a AMBE data do `/usr/share/dstarrepeater`,
- vytvoří systémového uživatele `dstarrepeater` a přidá ho do skupin
  `audio`, `spi`, `gpio`,
- zkopíruje vzorovou konfiguraci do `/etc/dstarrepeater/dstarrepeater.ini`,
- nainstaluje a spustí systemd jednotku `dstarrepeaterd.service`.

Manuální varianta:

```sh
make -j4 MQTT=0 GPIO=0
sudo make install MQTT=0 GPIO=0
sudo install -m 0644 deploy/dstarrepeater-zero2w.ini \
  /etc/dstarrepeater/dstarrepeater.ini
sudo install -m 0644 deploy/dstarrepeaterd.service \
  /etc/systemd/system/dstarrepeaterd.service
sudo systemctl daemon-reload
sudo systemctl enable --now dstarrepeaterd
```

## 5. Konfigurace

Uprav `/etc/dstarrepeater/dstarrepeater.ini`. Nejdůležitější hodnoty:

```ini
[Modem]
Type=SX1255

[SX1255]
RXDevice=hw:GenericStereoAu,1,0
TXDevice=hw:GenericStereoAu,0,0
SPIDevice=/dev/spidev0.0
RXFrequency=430975020
TXFrequency=438575000
RXShift=-25000
TXDeviation=1200
```

Reset pinu na Pi Zero 2 W (BCM GPIO_25):

```ini
# preferovaná cesta přes libgpiod
ResetChip=/dev/gpiochip0
ResetLine=25
# legacy sysfs fallback (globální číslo = 512 + 25)
ResetGPIO=537
```

Na Pi Zero 2 W je GPIO čip `pinctrl-bcm2835`/`bcm2835-gpio` na
`/dev/gpiochip0`; globální sysfs číslo GPIO_25 je `537`. Nenastavuj hodnoty
z Raspberry Pi 5 (`/dev/gpiochip4`), tam je číslování jiné.

## 5b. PA PTT (GPIO17)

Pro spínání externího koncového zesilovače přidej:

```ini
PTTChip=/dev/gpiochip0
PTTLine=17
PTTInvert=0
```

GPIO_17 se sepne při vysílání a rozepne po jeho skončení. `PTTInvert=1` použij
pro aktivní-low PA. Prázdný `PTTChip` funkci vypíná.

## 6. Spuštění a ověření

```sh
sudo systemctl status dstarrepeaterd
sudo journalctl -u dstarrepeaterd -f
```

Při startu se v logu objeví řádky typu:

```text
Modem type set to "SX1255"
SX1255 modem started: RX=430975020 Hz (LO=430950020 Hz), TX=438575000 Hz
```

Pokud reset přes libgpiod funguje, uvidíš i:

```text
SX1255: reset via libgpiod on /dev/gpiochip0 line 25
```

Otestování SPI a verze čipu lze provést i ručně pomocí `sx1255-spi` z
projektu `sx1255-fm-repeater`.

## 7. Křížová kompilace (volitelné)

Pi Zero 2 W umí běžet v 64bitovém (`arm64`) i 32bitovém (`armhf`) módu.
Pokud je na zařízení 64bitový Raspberry Pi OS, je nejspolehlivější přeložit
přímo na zařízení. Pro 32bitový OS lze křížově kompilovat z jiného stroje
např.:

```sh
sudo apt-get install -y g++-arm-linux-gnueabihf
make CXX=arm-linux-gnueabihf-g++ MQTT=0 GPIO=0
```

Binárka je pak dynamicky linkovaná proti ALSA/libusb na cílovém systému.

## 8. Troubleshooting

- **`Cannot open SPI device /dev/spidev0.0`** – povol SPI (`dtparam=spi=on`),
  zkontroluj `/dev/spidev0.0` a členství uživatele ve skupině `spi`.
- **`Cannot open RX I/Q capture device`** – overlay `genericstereoaudiocodec`
  není aktivní; zkontroluj `aplay -l` a název karty `GenericStereoAu`.
- **reset se neprovede** – ověř `ResetChip`/`ResetLine`. Na Pi Zero 2 W je
  správně `/dev/gpiochip0` a `25`; legacy sysfs číslo je `537`.
- **`Cannot create PID file`** – démon píše lock do `$DSTAR_RUN_DIR`; jednotka
  nastavuje `/run/dstarrepeater` přes `RuntimeDirectory=`. Při ručním spuštění
  bez systemd nastav `DSTAR_RUN_DIR=/tmp` nebo spusť jako root.
