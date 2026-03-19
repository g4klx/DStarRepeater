# Changelog

Newest changes first.

## 2026-03-19

- Removed wxWidgets dependency; ported to standalone C++17
- Added cross-platform support (Linux, Windows, macOS)
- INI-style config file format (MMDVMHost compatible)
- Config-driven logging with file, display, and MQTT level controls
- Config file path as required command-line argument
- MQTT enabled by default; GPIO auto-detected on ARM Linux
- Comprehensive code quality, security, and threading safety fixes
- Restored DTMF command execution via system()
- Fixed getControl() parameter ordering bug (commands 3-6 swapped)
- Added missing GMSK interface type config parsing
- Full codebase documentation and comments

## 2018-07-10

- Add error recovery to the Icom Terminal and Access Point modes.

## 2018-07-03

- Add Icom Terminal and Access Point modes.

## 2018-05-10

- Port to wxWIdgets-3.0.x

## 2015-10-12

- Change to the Windows serial controller for Windows 10.
- Add overflow warnings for the MMDVM.
- 2015xxxx
- Fix the TX Delay setting for the MMDVM.

## 2015-10-01

- Added the RX and TX levels to the MMDVM driver and configuration.
- Change the slow data header replacement in Gateway mode to be more intelligent.
- Add sync regeneration to the sound card repeater driver.
- Change the slipped sync detection in the sound card repeater driver.
- Use a new matched filter in the GMSK modem.
- More MMDVM driver changes.

## 2015-07-09

- Allow for a late arriving data sync in the sound card repeater controller.
- Add support for D-Star operation with the MMDVM.
- Fix bugs in the DV-RPTR V1, V2, V3, and DVMEGA drivers.

## 2015-06-15

- Rearrange the source code slightly.
- Allow a DVMEGA to be even slower at responding the first time.

## 2015-04-04

- Added method to ALSA interface for TX control in the sound card controller.

## 2015-03-08

- Added a .mk file for the Pi2.
- Increased the network timeout from one second to two seconds.
- More changes to the ALSA interface.

## 2015-02-13

- Updated the ALSA handling once again.
- Fixed a compile bug when defining GPIO.

## 2015-02-10

- Updated the dynamic serial port selection for /dev/ttyACM*
- Updated the ALSA handing.

## 2015-02-08

- Added ALSA for use on Linux, PortAudio is used elsewhere.
- Dynamic serial port selection on Linux.
- Allow for cross-band operating on the DVMEGA.
- GPIO is potentially available on more platforms.
- Assert the RTS pin for the DVMEGA at startup.
- Getting the version at the startup of a DVMEGA is now more aggressive.
- Added a sanity check to the reply from the DVMEGA.
- Improved configuration and validation for the DVMEGA.

## 2014-05-21

- Added DVAP error dump.

## 2014-05-20

- Switch on internal and external speakers for XDVRPTR firmware/hardware.
- DVMEGA messages say DVMega now.
- Added power setting to the DVMEGA radio.
- Add extra TX and RX entries for the Split driver, but not in the GUI.
- Add two extra commands.

## 2014-04-24

- Added /dev/ttyACM* for all serial devices under Linux.
- Added PTT Inversion control for external hardware controllers.
- Hold off the next D-Star transmission until driver is clear of previous data.

## 2014-04-15

- Change the DV-RPTR V2 & V3 handling back to the way it is done in the DV-RPTR Repeater.
- Fixed watchdog timer handling in TX Only and TX and RX modes.
- Reduce buffering in TX Only and TX and RX modes for local connections.

## 2014-04-10

- The statistics in the Split driver include all configured receivers.
- Put the type of the repeater into the title bar of the GUI.
- The first registration packet is after 10 seconds, and 30 seconds thereafter.
- Fix many bugs in the Split driver.

## 2014-03-06

- Removed the modem watchdog.
- Fixed the DV-RPTR V2 and V3 modem drivers.
- Fixed the DVMEGA driver.
- Fix the network name going missing the config program.

## 2014-03-03

- Experimental DC offset removal in the sound card driver.
- Use names in the Split driver to allow for easier matching of addresses and ports.
- Increase the number of receivers in the Split driver to five from three.
- Added a modem watchdog to the DV-RPTR, DVAP and DVMEGA modem drivers.
- Hold off writing the header until the TX is off for the DV-RPTR and DVMEGA modems.

## 2014-02-17

- Add extra logging to the Split driver.
- Remove the FEC restoration in RX only and RX and TX modes.
- Added TX Tail to the sound card driver.
- Changed the timings in the GMSK driver.
- Added GMSK driver modem reopen.
- Write the configuration to a file under Windows.

## 2014-02-08

- GMSK Modem changes.
- Improve the DVAP transmit handling.
- Added the Split controller.

## 2014-01-29

- Move the D-Star Repeater to its own package.
- More GMSK Modem changes for better locking.

## 2014-01-28

- Tweaks to the GMSK Modem controller in the D-Star Repeater.
- More work on buffering in the D-Star Repeater for all modem types.

## 2014-01-18

- Fix endian-ness of the DVMEGA frequency command.

## 2014-01-17

- Removed the DVAP Node and DV-RPTR Repeater.
- Block Australian foundation class and French novice callsigns from using the repeaters.
- Removed the optional delay on the DV-RPTR V1 modem.
- Added the DVMEGA hardware.

## 2013-12-31

- More cleanups.
- Removed the white list functionality.
- Added explicit support for the DV-RPTR V3 modem.

## 2013-09-04

- Remove even more of the C++ changes.

## 2013-09-03

- Removed the DC blocker from the sound card based repeaters.
- Remove some of the C++ changes.

## 2013-08-31

- Make DV-RPTR V2 handling better.
- Catch exceptions and other C++ changes in the D-Star Repeater.
- Add TX Delay to the DV-RPTR V2 modem in the D-Star Repeater.

## 2013-07-26

- Restore D-Star Repeater window position on startup.
- Allow for the error reply to be disabled.

## 2013-07-02

- Added daemon info to the log.
- Changes to the internal buffering.
- Remove the Analogue Repeater from the Repeater package.
- Remove the DCSGateway, DExtraGateway, and ParrotController from the Repeater package.
- Add a DC blocker to the sound card GMSK demodulator.
- Implement GMSK PLL locking.

## 2013-06-22

- Added XRF310.
- Try overlapped serial I/O under Windows again.
- Add the serial port PTT control to the D-Star Repeater.
- Add /dev/ttyS* devices to the serial control list.
- Added a receive level setting to the sound card driver in the D-Star Repeater.
- Fixed bugs in the D-Star Repeater regarding end of incoming transmissions.

## 2013-06-15

- Added XRF250.
- Add an optional startup delay for the DV-RPTR V1.
- Add a sound card interface to the D-Star Repeater.

## 2013-05-23

- Fixed GMSK Modem bug in the D-Star Repeater.
- Updated XRF008s IP addresses.
- Added XRF727.

## 2013-05-15

- Removed aggressive DVAP serial port opening.
- Remove support for the DUTCH*Star 0.1.00 firmware.
- Add GMSK modem support to the D-Star Repeater.
- Added Arduino hardware support.

## 2013-05-08

- Revert most of the serial port changes.

## 2013-05-03

- Tweaks to the DVAP handling.
- Made opening the serial port more aggressive for the DVAP.
- Included the D-Star Repeater and Config program in the Windows package for the first time.

## 2013-04-23

- Change to the wiringPi library rather than bcm2835 library for controlling the RPi.
- Added the generic D-Star Repeater and configuration program.
- Allow incoming radio audio during the valid_wait period.
- Don't reset the announcement timer on transmissions.
- Added XRF851.

## 2013-03-23

- Updated XRF003s IP address.
- Added the recording and playback of announcements.

## 2013-03-15

- Added XRF038.
- Allow for the new format of Danish callsigns.

## 2013-03-09

- Allow STN* callsigns as the my callsign.
- Allow for switching DTMF blanking off for duplex.

## 2013-03-02

- Make the DVAP initialisation less aggressive.
- Updated to match ircDDB Gateway 20130302.
- Added XRF150

## 2013-02-19

- Updated XRF008s and XRF123s IP addresses.
- Reset the DVAP into GMSK mode if FM data is received.
- Fix for GMSK modems request for space.
- Added DCS021.
- Added XRF333.

## 2013-02-03

- Remove DVAP frequency offset setting.

## 2013-01-31

- Upgrade the DCS Gateway to the latest protocol version.
- Revert DVAP polling change.

## 2013-01-27

- Allow for temporary ack text.
- Convert the DCS hosts files to use hostnames.
- Increase DVAP polling speed to increase reliability on slow machines.
- Fixed bug in status reporting.

## 2013-01-11

- Make the frame wait time configurable in the Split Repeater.
- Allow for empty beacons in the Analogue Repeater to suppress them.
- Convert old DV-RPTR configuration values (Windows only).
- Added DCS019.

## 2012-12-20

- Added DCS020.
- Updated XRF008s IP address.
- Add Ethernet support for the DVRPTR-V2 hardware.
- Add support for the Raspberry Pi GPIO port.

## 2012-11-12

- Removed XRF018 and XRF250.
- Updated DCS007s IP address.
- Added new XDVRPTR data transfer mode for v0.72 firmware or later.

## 2012-10-27

- Added DCS016, DCS017, and DCS018.
- Changed the announcement audio for B and D for UK English.
- Removed the beacon in Gateway mode.

## 2012-10-04

- Put the DCS and DExtra Gateways back into the package.
- Updated DCS protocol.
- Added support for the 70cms DVAP.
- Use the DVAP to validate its own frequency.
- Add a pause in the GMSK Repeater before PTT is set to on.

## 2012-09-20

- Make LibUsb on both Windows and Linux more agressive at transferring data.
- Remove the need for the extra thread in the Linux command line versions.
- Initial support for the XDV-RPTR (V2) protocol.
- Removed the DCS and DExtra Gateways.
- Added SVN revsion version to the log file.
- Add AMBE logging to non-GUI versions of the repeaters.
- Add -audiodir command line option to change the location of the AMBE data logging.

## 2012-09-01

- Allow module E on all of the repeaters.
- Re-added Windows LibUSB support to the GMSK Repeater.

## 2012-08-26

- Upgraded to Visual C++ 2010 on Windows.
- Platform dependent settings moved to Makefile includes.
- Changed the D-Star Repeater to the Sound Card Repeater.
- Upgraded to the latest version of the DCS protocol.
- Increased the default TX delay in the DV-RPTR Repeater to 150ms from 30ms.
- 20120826a
- Reverted to using Visual C++ 2008 on Windows.

## 2012-08-02

- Added DCS014 and DCS015.
- Removed XRF100.
- Fixed ARM cross compilation location of data files.
- Changed silence insertion parameter.
- Allow for up to four commands.

## 2012-07-06

- More GMSK Repeater rollbacks.
- Allow cross compilation to ARM processors.
- Added DCS011.

## 2012-06-15

- Added DCS013.
- Roll back GMSK repeater changes regarding driving the modem.
- Fixed background data in the GMSK Repeater.

## 2012-06-04

- Remove the GMSK debugging statements.
- More GMSK repeater changes.
- Fix bug in TX and RX mode in all but the D-Star Repeater.

## 2012-06-03

- Lot of debugging statements in the GMSK drivers.
- More GMSK repeater changes.
- Removed XRF009, XRF010, XRF011, XRF030, XRF038, and XRF110.
- Updated XRF031s and XRF069s IP address.
- Added DCS012.

## 2012-06-02

- Remove the LibUsb option for the GMSK modem in Windows.
- Infill data gaps with the last transmitted audio or silence.
- More GMSK repeater changes.

## 2012-05-27

- Improvements to the changes made in 20120526.
- 20120527a
- Minor tweaks to 20120527 relating to Windows mostly.
- Not generally released.

## 2012-05-26

- Run the GMSK Modem in a seperate thread.

## 2012-05-24

- Upgraded network error reporting.
- Added DCS010.
- Removed debug statements from the Slit Repeater.
- Add One-Touch Reply to the Dummy Repeater.

## 2012-05-11

- Fix the -nolog option for the Dummy Repeater.

## 2012-04-28

- Small revision to the DVAP driver.
- Add the -nolog command line option to the Dummy Repeater.

## 2012-04-24

- Remove the architecture setting in the top level Makefile.
- Added DCS004.

## 2012-04-20

- Upgrade the reopening of the DV-RPTR modem on Linux.
- Remove the first poll from the repeaters to the gateway.
- 20120420a
- Tweaks to the GMSK modem LibUSB driver to reduce the logging.
- Changes to the GMSK modem WinUSB driver.

## 2012-04-19

- Allow a greater range of GMSK modem addresses.
- Replaced NAtools with direct calls to WinUSB :-)
- Handle reopening of the DV-RPTR modem if it disconnects, Linux only for now.

## 2012-04-14

- Added DCS007.
- Handle reopening of the GMSK modem if it disconnects.

## 2012-04-12

- Fixed BER display on all of the repeater GUIs.
- Fixed header transmission bug in diversity modes.
- More changes to the Split Repeater.
- Not generally released.

## 2012-04-11

- Added Norwegian as a language to the DExtra and DCS Gateway.
- Added an optional voice beacon to all of the D-Star repeaters.

## 2012-04-03

- Sort the callsign entries in the Dummy Repeater.
- Minor cleanups.

## 2012-04-02

- Better error handling of the DV-Dongle.
- More changes to the Split Repeater.

## 2012-03-30

- Updated the DCS and DExtra hosts files to the latest versions.
- Convert the main log files to use UTC instead of local time.
- More changes to the Split Repeater.

## 2012-03-28

- Fixed ack bug in the Split Repeater.
- Added DCS006.

## 2012-03-27

- Add extra logging to the Split Repeater for unknown IP addresses and ports.
- Modify the Linux configuration system to fix a bug.
- Updated XRF017s IP address.
- Added DCS008.

## 2012-03-26

- Fixed bugs in the RF D-Star repeaters when used with the Split Repeater.
- Fixed bugs in the use of the Split Repeater with the RF repeaters.
- Added DCS005.

## 2012-03-24

- Added "TX and RX" mode to all of the D-Star repeaters except the DVAP Node.
- More changes to the Split Repeater.
- Added DCS009.

## 2012-03-22

- Added XRF030.
- Added the Split Repeater.
- Allow minimum ack time in the Analogue Repeater to be 100, 300, and 500ms.
- Add up to 100ms of squelch delay in the Analogue Repeater.

## 2012-03-20

- Updated XRF021s IP address.
- Added TX output to the K8055 with the GMSK and DV-RPTR repeaters.
- Added the DCS Gateway.

## 2012-03-12

- Added CM119A product id for latest URI USB hardware.
- Added K8055 support to the GMSK and DV-RPTR repeaters.

## 2012-03-10

- Updated the DV-RPTR driver again.
- Binary only release for Windows users.
- 20120310a
- Updated the DV-RPTR and DVAP drivers.

## 2012-03-09

- Changes to support the DCS reflector system.
- More changes to the DV-RPTR driver.

## 2012-03-08

- Changes to the DV-RPTR driver.
- Updated XRF021s and XRF044s IP address.
- Added XRF222, XRF353, and XRF777.
- Not generally released.

## 2012-03-03

- Added AMBE regeneration to background data.
- Added receive-only and transmit-only modes to the repeaters.

## 2012-02-28

- Restore GUI window position on startup.
- New repeater to gateway protocol implemented.
- Allow background audio data to be transmitted.
- Added a receive hang time to the Dummy Repeater.

## 2012-02-23

- Blank any DTMF tones from being transmitted over RF.
- Added audio delay and pre-, de-emphasis to the Analogue Repeater external port.

## 2012-02-21

- Changed the lower frequency limits of the analogue repeater callsign and ack.
- Added the E (Echo) and I (Info) commands to the DExtra Gateway.
- Updated XRF069s IP address.

## 2012-02-13

- Fixed D-Star Repeater Windows configuration bug.

## 2012-02-10

- Updated XRF021s IP address.
- Small cleanups.

## 2012-02-06

- Bug fixes for 20120205.

## 2012-02-05

- New configuration system for non-Windows platforms.
- Added XRF040.

## 2012-01-25

- Allow for the settings of the beacon text for the D-Star repeaters.
- Equalised the audio levels in the Dummy Repeater;
- Updated XRF019 IPs address.
- Added XRF110.

## 2012-01-16

- Updated XRF033s IP addeess.
- Fixed GUI PTT bug in the Dummy Repeater.
- Add an audio resampler to the Dummy Repeater.

## 2012-01-11

- Increase maximum D-Star Repeater hang time to 3000ms from 2000ms.
- Improve gateway mode rejection rules.
- Allow the Dummy Repeater to be controlled from external hardware.
- Added XRF119.

## 2011-12-31

- Added the upgraded Dummy Repeater to the repeater package.
- Allow channel D in the DExtra Gateway.

## 2011-12-22

- Allow the ack on the D-Star repeaters to be selectable for BER or status.

## 2011-12-21

- More work on the Analogue Repeater radio audio to the external port.

## 2011-12-20

- More work on the Analogue Repeater radio audio to the external port.
- Updated XRF031s IP address.

## 2011-12-15

- More work on the Analogue Repeater radio audio to the external port.

## 2011-12-13

- More work on the Analogue Repeater radio audio to the external port.

## 2011-12-12

- In the Analogue Repeater send radio audio to the external port when relaying from the external port.
- Removal of the USAGE.txt file.
- Fix an ancient bug with the output switching in the D-Star Repeater.

## 2011-12-09

- Change the ack text to include the reflector instead of the time.
- Updated XRF019s IP address.

## 2011-12-08

- Fix bug in the D-Star Repeater for the new status messages.
- Similar fix to the DVAP Node.
- Small change to the new ack format.

## 2011-12-07

- Acks changed from the link status to the transmission time and BER.
- Allow up to five status messages.

## 2011-12-04

- Allow headers received when in full duplex and transmitting network data to be passed to the gateway.

## 2011-11-26

- Changes to the DV-RPTR modem startup for slow serial subsystem starts.
- Dummy gateway handling added.

## 2011-11-25

- Handle startup of the DV-RPTR modem and DVAP better.

## 2011-11-22

- Allow more choices for the analogue repeater Ack delay.
- Cleaned up the DV-RPTR Modem settings.
- Updated the Windows NATools DLL.
- Updated XRF002 IP address.
- DExtra Gateway fix.

## 2011-11-20

- Changes to the DVAP Node and DV-RPTR Repeater handling.

## 2011-11-18

- Increased the GUI width on Linux to accomodate the new status messages.
- Allow for the DV-RPTR TX Delay up to 500ms from 250ms.
- Fixed the config file name for the DV-RPTR daemon version.
- Fixed the DV-RPTR communications protocol.
- Fixed DV-RPTR version string.
- Updated XRF069 IP address.
- Added XRF038.

## 2011-11-12

- Fix to the DExtra Gateway.
- Minor bug fix.

## 2011-11-09

- Output buffer changes.
- Added support for the reporting of the two status messages.

## 2011-11-02

- More changes to the DV-RPTR protocol.
- The default log directory is now the home directory under Linux.
- Bug fix to the DExtra Gateway.
- Increased network security for the repeaters.
- Added XRF123.

## 2011-10-22

- Changes to the DV-RPTR protocol.
- Added packet logging for local traffic for all DV repeaters.
- Pass the headers from the DV repeaters through properly.
- Increase the size and number of the network queues.
- Changed XRF069 IP address.
- Changes to the repeater to gateway protocol.

## 2011-10-16

- Added XRF069.
- Reverted external input handling for the Analogue Repeater.

## 2011-10-04

- Changed how data is fed to the DVAP and DV-RPTR.
- Incorporated the new DV-RPTR data formats.

## 2011-09-23

- More changes to the DV-RPTR Repeater.
- Changed XRF073 IP address.

## 2011-09-15

- Revert the FTDI change for the DVAP on Windows.
- Changes to the queueing.
- Enable the debug flag on Linux builds.
- More changes to the DV-RPTR Repeater.

## 2011-09-13

- Use the official FTDI interface to access the DVAP on Windows.
- Changed XRF002 IP address.

## 2011-09-07

- More internal changes to handle extreme situations.

## 2011-09-05

- Considerable internal changes to the output queueing.
- Bug in the silence inseration fixed.

## 2011-09-02

- Seperate the closdown ID from the periodic ID in the Analogue Repeater.
- Lots of internal changes to the DVAP Node.
- More work on the DV-RPTR Repeater.

## 2011-08-31

- More work on the DV-RPTR Repeater.

## 2011-08-30

- Fix Squelch and Noise display when transmitting in the D-Star Repeater.
- First untested version of the DV-RPTR Repeater.

## 2011-08-26

- Allow for alternative GMSK Modem addresses in NAtools.
- Use the new silence insertion in the D-Star Repeater.

## 2011-08-25

- Added a noise level display to the D-Star Repeater.
- Added the Squelch Mode to the D-Star Repeater.

## 2011-08-23

- Fixed a bug in the DExtra Gateway.
- Allow the opening callsign of the Analogue Repeater to be different from the closing callsign.
- Fixed the minimum time for ack timer in the Analogue Repeater.
- Added XRF073.

## 2011-08-22

- The Battery Ack is now configurable in the Analogue Repeater.
- Fix to the Windows SerialLineController.cpp
- Add more buffering to the D-Star Repeater.

## 2011-08-18

- The RX and TX Levels affect the D-Star Repeater in real-time.
- The levels and thresholds in the Analogue Repeater operate in real-time.
- Increased the Analogue Repeater hang timer maximum to 30 seconds.
- All of the IDs and Acks in the Analogue Repeater can now be WAV files.

## 2011-08-16

- Allow the timers to handle long timeouts correctly.
- The Squelch Level affects the D-Star Repeater in real-time.
- Change the serial port handling for the Analogue and D-Star repeaters.

## 2011-08-14

- Added the noise squelch to the D-Star Repeater.
- More work on GMSK full duplex mode.

## 2011-08-11

- Minor internal changes to all of the repeaters.
- Improved timing for the DExtra Gateway and Parrot Controller.
- Added fuzzy matching for the frame sync to the D-Star Repeater.
- 20110811a
- Fix for GMSK Repeater full duplex mode.
- Identify the OS and wxWidgets version in the log for all programs.

## 2011-08-08

- First version of the DVAP Node program.
- Allow incoming network data during wait periods.
- Clean up GMSK Repeater local repeating.
- 20110808a
- Serial port selection bug fixed.
- 20110808b
- Windows serial port changes.

## 2011-07-20

- Fix PTT bug in 20110719.

## 2011-07-19

- Removed the Network Delay from the D-Star Repeater.
- Added extra delay and buffering to the D-Star Repeater for network traffic.

## 2011-07-17

- Cleans-ups in all the repeaters.
- More work on 0.1.00 stability.

## 2011-07-16

- Added Nederlands as a language option to the DExtra Gateway.
- Removed the cycle time option for the GMSK Repeater.
- First version that is stable with DUTCH*Star 0.1.00 firmware.

## 2011-07-14

- Insert a 100ms delay before sending the data after sending the header.
- Updated XRF033.

## 2011-07-12

- Improve the clock accuracy in the GMSK Repeater.

## 2011-07-11

- Handle DUTCH*Star 0.1.00 firmware explicitely.

## 2011-07-10

- Turned the logging upside down to match the gateway, and common sense.
- Improvements to the NAtools drivers.

## 2011-07-08

- Upgraded the LibUSB driver from libusb-0.1 to libusb-1.0 under Linux.
- Upgraded the NAtools version under Windows.
- Removed NAtools on the Linux side.

## 2011-07-06

- Changed timings when dealing with LibUSB and relaying from the modem.
- Changed Modem Type to Firmware Type.

## 2011-07-04

- Allow for compilation without NAtools being available.
- Added XRF028.

## 2011-07-03

- Trying to handle unreasonable combinations of modem and USB chipset.

## 2011-07-02

- Remove the need to have LibUSB installed on Windows.

## 2011-06-28

- Remove the need to have NAtools installed on Windows.

## 2011-06-22

- Clean ups and removal of some logging messages.
- Added extra delays for libUSB.

## 2011-06-21

- Increased the max cycle time value to 20ms.
- Now write 12 bytes per cycle instead of 8.
- 20110621a
- Changed from the broken node_putdta to node_io.

## 2011-06-13

- Display cycle overruns more intelligently.
- Display time taken for the networking to complete.
- Stop polling the URI USB in the D-Star Repeater, it's not needed.

## 2011-06-10

- Added more cycle times for the GMSK modem.
- Added an extra serial port config.

## 2011-06-09

- Add the cycle time setting to the GMSK modem settings.
- Added different serial port configs to allow for different serial port settings.

## 2011-06-01

- Small GMSK modem change, rolls back another change made in 20110529.

## 2011-05-29

- Tweaks to the software type/version reporting to the gateway.
- 20110529a
- Small GMSK modem change, rolls back a change made in 20110529.

## 2011-05-25

- Updated the IP address of XRF020.
- Changed poll format to report the repeater type to the gateway.
- Small change in timing of the GMSK Repeater.

## 2011-05-12

- Updated the USAGE.txt file to include the -daemon option.
- The pid of the programs is written to /var/run for Linux versions.
- Added the -confdir option to the daemon versions of the programs.

## 2011-05-10

- Fundamental change to the GMSK Modem handling.
- Cleaned up 20110508.

## 2011-05-08

- Added -daemon option to create proper daemons on Linux.
- Not generally released.

## 2011-05-06

- Added XRF031.
- Added the ack time to the minimum time for the analogue repeater controller.
- Allow outgoing DExtra links to have random ports.
- Change timings in the GMSK Repeater.

## 2011-04-21

- Fixed an obscure bug in the Parrot Controller.
- Updated the IP address of XRF021.
- Change timings and data feed to the GMSK Modem.

## 2011-04-20

- Changed the timing of feeding the data to the GMSK Modem.

## 2011-04-18

- Added XRF033.
- Updated the IP address of XRF013.
- Changed the timing in the main loop.
- Not generally released.

## 2011-04-17

- Changed from wxWidgets 2.8.11 to 2.8.12.
- Small internal timing tweak for the GMSK Repeater.

## 2011-04-13

- A small tidy up of the previous release.

## 2011-04-11

- Different method of silence insertion used.

## 2011-04-04

- More mutex locking changes in the GMSK Repeater.
- Timing changes in the GMSK Repeater.

## 2011-04-03

- Mutex locking in the last couple of releases was a little wrong.

## 2011-04-02

- Refinements to 20110331 based on feedback.

## 2011-03-31

- Timing changes in the GMSK Repeater.
- Put the networking back into the second thread on the GMSK Repeater.

## 2011-03-29

- Relax the header checksum testing under certain circumstances.
- Check the incoming my call with a regular expression.
- More buffering of network data in the GMSK Repeater.

## 2011-03-27

- Removed the bias patch which was added to the 20110308 release.
- Removed the silly GMSK Repeater error messages that I added.
- Updated the IP address of XRF008.
- Added extra my callsign validation to the repeaters.

## 2011-03-08

- Added daily logs.
- Added the -logdir command line option to the programs.
- Added XRF007 and XRF021 to the DExtra_Hosts.txt file.
- Added the sound card demodulator bias patch from Eric NS7DQ.
- 20110308a
- Timing changes to the GMSK Repeater closer to the settings of 20110219a.

## 2011-03-01

- Remove a delay added to the NAtools driver.
- Fixed Linux compile error with the previous release.

## 2011-02-27

- Added optional RPT1 validation.
- Relaxed the timing when talking to the GMSK modem.

## 2011-02-22

- Added Swedish and Spanish translations to the DExtra Gateway.
- Increased the USB timeout when using libUSB with the GMSK modem.
- Increased the packet silence timeout from 30ms to 40ms.
- Run the network processing from the timer thread in the GMSK Repeater.
- 20110222a
- Small change to the GMSK Repeater to stop lock-ups.

## 2011-02-19

- Swapped the callsigns in the D-Star Repeater to match the GMSK Repeater.
- Removed support for Dutch*Star 1.00.0 firmware.
- 20110219a
- Re-arranged the transmitted callsigns in both of the digital repeaters.
- Validate the incoming network header for both digital repeaters.

## 2011-02-17

- Fixed bug in DExtraGateway with incoming headers disrupting communications.
- Change the maximum Parrot Controller beacon timer to 20 minutes.
- More GMSK Repeater internal changes.

## 2011-02-15

- Added XRF020 to DExtra Gateway.
- Added Dutch*Star 1.00.0 firmware compatibility again.

## 2011-02-13

- Internal restructuring of the GMSK Repeater.
- Removed the RX State from the GMSK Repeater GUI.

## 2011-02-10

- Handle errors in the GMSK modem better.
- More timing changes to the GMSK Repeater.

## 2011-02-08

- More timing changes to the GMSK Repeater.

## 2011-02-05

- Fixed the callsigns in the acks of the GMSK Repeater.
- Change the timings, particularly when handling receive on the modem.
- Change the default ports to match those of ircDDB Gateway.

## 2011-01-23

- More timing changes for the GMSK Repeater.

## 2011-01-21

- More timing changes for the GMSK Repeater.

## 2011-01-20

- Added XRF000 for the DExtra Gateway.
- DExtra protocol has been upgraded.
- Another timing change for the GMSK Repeater receiver.

## 2011-01-16

- Swap RPT1 and RPT2 for the NAtools modem driver.
- Increase the polling rate in the GMSK Repeater.
- Not generally released.

## 2011-01-14

- Swap RPT1 and RPT2 for the libUSB modem driver.
- Different GMSK modem buffer filling routines.
- Not generally released.
- 20110114a
- Same as 20110114 but with additional debugging information.
- Not generally released.

## 2011-01-09

- Allow for the use of both the libUSB and NAtools interface.
- Change how the internal timing operates.
- Not generally released.
- 20110109a
- Increase the buffer size and put in a cycle of silence to start it off.
- Not generally released.

## 2011-01-08

- Replaced the DExtra_Hosts.txt with the one from the ircDDB Gateway package.
- More changes to buffering and timing in the GMSK Repeater.
- Not generally released.
- 20110108a
- More changes to buffering and timing in the GMSK Repeater.
- Not generally released.

## 2011-01-07

- Removed the statistics from the GMSK Repeater.
- Fixed the Linux compile bug from 20110105.
- Added a PTT maintainer for the GMSK Repeater.
- 20110107a
- A change to the PTT maintainer which may be more effective.
- Increased the buffer size for RF data.
- 20110107b
- Return to the PTT system of 20110107.
- Increase the buffer size for the RF data again, it is now 50 frames.
- Not generally released.

## 2011-01-05

- Added the PTT Hang parameter to the D-Star Repeater.
- Added the XRF019, XRF026, XRF027, and XRF044 reflectors.
- Revert to using libusb.

## 2010-12-12

- Test a new version of the libusb GMSK Repeater.
- Clean up the GUI a little for the Flags and BER.

## 2010-12-11

- The returned BER was wrong, it should have been the number of error bits.
- The DExtra Gateway now reports the link status correctly to the repeaters.
- Added French, German, Danish, Italian and Polish to the DExtra Gateway ack languages.
- Added the XRF055 reflector.
- Back to using NAtools.

## 2010-12-08

- Converted the GMSK Repeater to use libusb instead of NAtools.
- Changed to a new extension protocol to pass data to/from a gateway.
- Collect extra statistics and put the AMBE BER on the GUI.
- Added a statistics packet.

## 2010-12-03

- Another stab at full-duplex on the GMSK Modem.
- Handle out-of-order data on the network traffic.
- 20101203a
- Changed to wxWidgets 2.8.11.
- Fixed half/full duplex modem setting.

## 2010-12-02

- Fix the statistics for all the repeaters, hopefully.
- Changed the callsigns in the acks to be Icom compatible.
- Added more intelligent silence insertion.

## 2010-11-21

- Added the Active output for the URI USB at the expense of GUI Output 2.
- Changed the end of transmission network packet to be more standard.
- More changes to the GMSK Repeater.

## 2010-11-13

- Changed the timing within the GMSK Repeater.
- Fix a problem with receiving text from the latest ircDDB Gateway.

## 2010-11-12

- Logs are now written into the /var/log directory under Linux.
- Removed the writer thread and gone back to single threading.
- Added the Intel.o file for i386 to allow the GMSK Repeater to build.

## 2010-11-10

- Another try, but still not working properly

## 2010-11-09

- More changes to the GMSK Repeater.
- 20101109a
- Plan B for getting full duplex working with the GMSK Repeater, a seperate thread for the transmitter.

## 2010-11-07

- More changes to the GMSK Repeater.
- Add the -gui command line flag to all the programs.

## 2010-11-04

- Changes to the GMSK Repeater.
- Pass Flag2 and Flag2 through the D-Star Repeater.
- The GMSK Repeater can now build on an AMD64 Linux machine.

## 2010-10-26

- Changed the way the header is received in the GMSK Repeater.
- Increased the watchdog timer to two seconds from half second.

## 2010-10-16

- Allow use of shared libraries on Linux.
- Fixed header transmission for the GMSK Repeater.
- Reject relayed gateway-ed data to stop confusing G2.

## 2010-09-24

- Added display of the ack text to the repeater GUIs.
- Use Fred PA4YBR's modem support library for GMSK Modems.

## 2010-09-22

- More callsign changes in the repeaters and Parrot Controller.
- Changes timings for the GMSK Modem.
- Not generally released.

## 2010-09-17

- More callsign changes.
- More work on the GMSK Modem support.
- Not generally released.

## 2010-09-15

- Allow for the choice of the GMSK modem type, Satoshi or Dutch*Star. Only the
- Satoshi option works as expected.
- Added text from the DExtra Reflector to appear in the repeater ack and beacon.
- More changes for the GMSK Modem, it appears even better now.
- A number of changes to the transmitted callsigns made.
- Not generally released.

## 2010-09-07

- The GMSK Repeater now works on a Satoshi 7.08 system.
- Not generally released.

## 2010-09-03

- Modify the slow data when in gateway mode.
- Not generally released.

## 2010-09-01

- Added optional callsign restriction to both D-Star repeaters.
- Added repeater callsign to the DExtra Gateway configuration, as this fixed a
- bug.
- Not generally relased.

## 2010-08-26

- Linux support for the DExtra Gateway.
- Written documentation for the DExtra Gateway.

## 2010-08-25

- First working version of the DExtra Gateway.
- Not generally released.
- 20100825a
- Bug fixes to the DExtra Gateway.
- Not generally released.

## 2010-08-23

- Added the CTCSS hang timer.
- Not generally released.

## 2010-08-20

- Added the DExtra Gateway.
- Added hourly usage statistics to all of the repeaters.
- Not generally released.

## 2010-08-18

- Added the GMSK repeater.
- Not generally released.

## 2010-08-15

- Re-integrated the configuration for both repeaters back into the main
- repeater programs.
- Fix startup and shutdown when PTT inversion is set.
- Not generally released.

## 2010-08-13

- Added a beacon to the D-Star repeater.
- Fixed bug in the new slow data decoder.

## 2010-08-11

- Improvements to the slow data decoder.

## 2010-08-10

- Added 79.7 Hz as a CTCSS tone.
- Added an RX Level setting to the D-Star repeater for when running alongside
- analogue for adjusting input levels.
- Bug in the slow data decoder fixed.
- Not generally released.

## 2010-08-08

- Fix to the DVTool files.
- Fixed shutdown command.
- PortAudio parameter depends on the operating system, new parameter for Linux,
- old for Windows.

## 2010-07-26

- Added new CTCSS output modes.
- Changed the parameter of PortAudio which appears to make a big difference under
- Linux.
- Not generally released.

## 2010-07-20

- More bug fixes.
- CTCSS goes off before sending the callsign in lockout.

## 2010-07-19

- A bug fix on the previous release.

## 2010-07-12

- Better handling of error conditions at startup on the repeaters.
- Added new time to limit the timeout tones on the analogue repeater.
- Change sound card handling times.
- Not generally released.

## 2010-06-10

- Added the end of transmission senc at the end of the network transmision.
- Lengthened the D-Star transmit period.
- Not generally released.

## 2010-06-04

- Bug fixes.
- Re-implement the audio delay as negative PTT delay.
- Not generally released.

## 2010-06-03

- Bug fixes.
- Use the ack timer to pause the D-Star repeater in gateway mode between overs.
- Added an optional audio delay to the D-Star output audio.
- Not generally released.

## 2010-05-16

- Extra D-Star header validation.

## 2010-05-10

- A bit of a stinker, best forgotten.
- Not generally released.

## 2010-04-24

- Threading bug in 20100423 fixed.
- Not generally released.

## 2010-04-23

- Moved output controls to the menu bar instead of buttons.
- Display the last three lines of the log on the repeater screen.
- Not generally released.

## 2010-04-19

- Special test release for GB3IN.
- Not generally released.

## 2010-04-15

- Re-added the ack timer, but it can now handle shorter times than before.
- Allow for the manual setting of the D-Star gateway callsign.
- Fixed a major bug in 20100412.

## 2010-04-12

- Changed ack selection to free form text in the analogue repeater.
- Removed the need for a squelch input or the audio delay setting for D-Star.
- Now takes the header from slow data if the header was missed.
- Removed the ack timer.
- Better handling of missing sync bytes on both the radio and network sides.

## 2010-02-23

- Bug fixes for 20100222.

## 2010-02-22

- Added a gateway callsign to the D-Star repeater for messages.
- Removed the Icom G2 Protocol Hander as it wasn't used.
- Allow for a -nolog command line option to disable all logging.
- Simplified the mode selection and added an ack option for D-Star.
- Change network validation to only be on the IP address.
- Not generally relased.

## 2010-01-14

- Bug fixes for 20100112.

## 2010-01-12

- Fixed Active output bug in 20100111 in the D-Star repeater.
- Added a Disable input to the URI USB.
- Allows both external commands for Startup and Shutdown and the Disable input.
- Not generally released.

## 2010-01-11

- Added menu commands to run Command 1 and 2 on the analogue repeater.
- Removed the timed command added in 20100107.
- Added a Disable input to shutdown and startup the repeaters.
- Modified the Active output to use with the new Disable input to allow cross linking of repeaters leading to true dual-mode operation.
- Fixed bug with K8055s with addresses greater than zero.
- Not generally released.

## 2010-01-08

- Fixed Linux compilation issues.

## 2010-01-07

- Added a command to be run at a period after the D-Star Repeater closes.
- Not generally released.

## 2010-01-06

- Added Watchdog timer to the Parrot Controller.
- Added suffix D for 10m to the D-Star Repeater.
- Added two external commands to both repeaters.
- Added the four outputs to the D-Star control to match the analogue Repeater.
- Not generally released.

## 2009-12-31

- Fixed bugs in the Parrot Controller.
- Added adjustable Turnaround Time to the Parrot Controller configuration.

## 2009-12-30

- First version of the Parrot Controller.
- Small change to the G2 protocols.
- Not generally released.

## 2009-12-29

- Fixed Linux build issues with 20091228.
- Included incomplete Parrot Controller.

## 2009-12-28

- Tweaks to the DTMF controller.

## 2009-12-26

- Added Parrot mode to the D-Star repeater.
- Added DTMF display to the analogue repeater.
- Not generally released.

## 2009-12-19

- Added DTMF and local commands to the analogue repeater.
- Added remote and local commands to the D-Star repeater.
- Added Callsign Holdoff to the analogue repeater.
- Removed the RPT2 validation that was added for the last release.
- Added new programs to configure the repeaters and removed configuration from the
- main programs.
- Added two new programs, rmk8055drv and rmuridrv, to remove the Linux usbhid
- driver.
- Fixed Simplex/Duplex choice bug.
- Not generally released.

## 2009-12-12

- Added the Gateway mode to the D-Star repeater.
- Don't pass through RF data with the RPT2 entry set to my callsign.
- Brought the K8055 drivers internal for both Windows and Linux.
- Put the ack back in simplex mode.
- Added the callsign on latch to the analogue repeater.
- Added a choice of timeout tones to the analogue repeater.

## 2009-11-26

- Bug fix release for network delay.
- Don't pass through RF data with the RPT2 entry set to blanks.
- Not generally released.

## 2009-11-25

- Changes to the "Open G2" network protocol.
- Added the network delay for the D-Star repeater.
- Not generally released.

## 2009-11-20

- Merged the callsign and beacon timers in the analogue repeater.
- Support the URI USB.

## 2009-11-17

- Added support for serial ports as I/O ports for both repeaters.
- Removed the ack when in simplex mode.
- Added all the EIA CTCSS tones to the analogue repeater.
- Changes to the "Open G2" network protocol.

## 2009-11-06

- A cleanup of the 20091104 release because it works so well and deserves to be
- released :-)

## 2009-11-04

- Changes to the protocol handlers to handle odd cases.
- Not generally released.

## 2009-11-03

- Move the log files to the users home directory.

## 2009-10-31

- More changes to the "Open G2" network protocol.
- Allow network data relaying while waiting for the ack.
- Not generally released.

## 2009-10-30

- More changes to the "Open G2" network protocol.
- Not generally released.

## 2009-10-29

- Implemented the "Open G2" network protocol.
- Not generally released.

## 2009-10-27

- Removed RPT2 validation from network received headers.
- More G2 changes.
- Not generally released.

## 2009-10-26

- Added a new bandpass filter to the analogue repeater to remove the CTCSS tones.
- More G2 changes.

## 2009-10-25

- Changed to the GMSK modulator to be more conventional.
- Added command line versions of the repeaters.
- Major updates to the G2 protocol.
- Not generally released.

## 2009-10-21

- Removed the confirmation prompt when exiting the repeaters.
- Not generally released.

## 2009-10-20

- Lots of Linux GUI cleanups.
- Added the Watchdog timer to the D-Star GUI.
- Added a simplex mode to the D-Star repeater.
- Added an output level to the D-Star repeater to allow for sharing with the
- analogue controller.
- Not generally released.

## 2009-10-17

- Allow for the configuration of the local IP address to bind to.
- Not generally released.

## 2009-10-16

- Added a network watchdog timer to handle the loss of the end packet.

## 2009-10-15

- Allow for named configurations to allow multiple repeaters on one machine.
- Increased the maximum PTT Delay to 500ms from 200ms.
- Added the PTT Delay and PTT and Squelch inversion to the Analogue Repeater.
- More changes to the G2 protocol handler.
- Not generally released.

## 2009-10-08

- Changes in the G2 protocol handler.
- Not generally released.

## 2009-10-02

- Removed RPT2 validation.
- Added PTT and Squelch inversion for the D-Star Repeater.
- Added G2 Gateway support.
- Tightened the filter before the GMSK democulator.
- Not generally released.

## 2009-09-23

- Validate the RPT2 callsign.
- Fixed bit errors on the D-Star Ack.
- Improved the documentation about the D-Star Repeater.
- Added a PTT delay for D-Star.

## 2009-09-17

- Linux cleanups.
- Changes to the D-Star Controller.
- Not generally released.

## 2009-09-16

- The External Transmit is only set when relaying radio audio.
- First version of the D-Star Controller.
- Optimised the K8055 communication.

## 2009-09-11

- Add a battery input and a change of Ack to signify battery operation.
- Not generally released.

## 2009-09-10

- Allow for an external CTCSS decoder.
- Not generally released.

## 2009-09-09

- Turn off the CTCSS early when the end callsign is enabled because of slow
- responding decoders in radios allowing some of the callsign through.
- Added an optional audio delay on the input to synchronise with the squelch
- input(s).
- Make GUI updates optional.

## 2009-08-24

- Audio levels are now reset before each received transmission instead of at the
- Ack.
- Added controls to toggle the unused output lines to control external hardware.
- Delay renamed to Ack in the analogue repeater GUI.
- Fixed bugs in the state machine causing timeout resets and a couple of missing
- state transitions.

## 2009-08-13

- Fixed audio out bug.
- CTCSS output is now for the whole time the repeater is accessed.

## 2009-08-12

- Fixed CTCSS output tones.
- Widened the CTCSS detector from 2Hz to 5Hz and from 500ms to 200ms.
- Numerous internal improvements.
- Not generally released.

## 2009-06-30

- Changed the main filter back to the FIR for performance reasons.
- First version to be used on a real repeater, GB3IN on 2m.

## 2009-06-29

- Switched off filters when not transmitting to reduce CPU load.
- Moved CTCSS insertion after pre-emphasis filter.
- Not generally released.

## 2009-06-26

- Converted the pre-emphasis, de-emphasis and audio filters to IIRs for efficiency
- and better shapes.
- Show VOGAD levels for the Radio and External on the main screen.
- Not generally released.

## 2009-06-19

- Added a VOGAD unit, and made the VOGAD on both the Radio and External audio.
- Not generally released.

## 2009-06-18

- Removed the icons from the programs.
- Increased the audio output for the external output.
- Not generally released.

## 2009-06-17

- The D-Star Repeater now compiles cleanly on Windows and Linux, so a good time to
- do a release.

## 2009-06-16

- Added the D-Star Repeater, but unfinished.
- Optimised access to the K8055.
- Not generally released.

## 2009-06-13

- Added Linux support, but not tested.
- Changed the name of the executable and directories.
- Changed how the K8055 parameters are set.
- Allow for COM ports higher than COM10.
- Not generally released.

## 2009-06-10

- Added the audio level value to the GUI.
- Re-arranged the GUI layout.
- Not generally released.

## 2009-06-09

- Added a simple VOGAD with reporting in the GUI.
- Reworked the filters and output levels.
- Not generally released.

## 2009-06-08

- Added debugging information in the tone decoder.
- Not generally released.

## 2009-06-07

- Silly bug in the through audio fixed.
- Added the option to use a sound file as an input, but only in a debug build.

## 2009-05-30

- Added audio filtering and optional pre- and de-emphasis. Also modified the
- configuration to have less pages as well as add the new settings.
