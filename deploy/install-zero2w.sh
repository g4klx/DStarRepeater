#!/bin/sh
#
# Install dstarrepeaterd with SX1255 support on a Raspberry Pi Zero 2 W.
# Author: Karel Kadlec (OK1LBC) <xkk@post.cz>
#
# Run on the target device as root (or through ssh).  The script builds the
# daemon, installs the binary/data, creates a dedicated runtime user and sets
# up the systemd service.
#
# Usage:
#   sudo ./deploy/install-zero2w.sh
#
# Optional overrides:
#   BINDIR, DATADIR, CONFDIR, RUN_USER, MAKE_OPTS

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(dirname "$SCRIPT_DIR")

BINDIR=${BINDIR:-/usr/bin}
DATADIR=${DATADIR:-/usr/share/dstarrepeater}
CONFDIR=${CONFDIR:-/etc/dstarrepeater}
RUN_USER=${RUN_USER:-dstarrepeater}
MAKE_OPTS=${MAKE_OPTS:-MQTT=0 GPIO=0}

if [ "$(id -u)" -ne 0 ]; then
	echo "This script must be run as root." >&2
	exit 1
fi

echo "==> Checking build tools"
for tool in make g++; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "Missing $tool. Install build dependencies first:" >&2
		echo "  apt-get install -y g++ make libasound2-dev libusb-1.0-0-dev" >&2
		exit 1
	fi
done

echo "==> Building dstarrepeaterd"
make -C "$REPO_DIR" -j"$(nproc)" $MAKE_OPTS

echo "==> Installing binary and data"
make -C "$REPO_DIR" install $MAKE_OPTS BINDIR="$BINDIR" DATADIR="$DATADIR" CONFDIR="$CONFDIR"

echo "==> Creating runtime user '$RUN_USER'"
if ! id "$RUN_USER" >/dev/null 2>&1; then
	useradd --system --no-create-home --shell /usr/sbin/nologin "$RUN_USER"
fi
for grp in audio spi gpio; do
	if getent group "$grp" >/dev/null 2>&1; then
		usermod -aG "$grp" "$RUN_USER"
	fi
done

echo "==> Installing configuration"
install -d "$CONFDIR"
if [ ! -f "$CONFDIR/dstarrepeater.ini" ]; then
	install -m 0644 "$SCRIPT_DIR/dstarrepeater-zero2w.ini" "$CONFDIR/dstarrepeater.ini"
	echo "    wrote $CONFDIR/dstarrepeater.ini (edit frequencies/callsign)"
else
	echo "    $CONFDIR/dstarrepeater.ini already exists, leaving it unchanged"
fi

echo "==> Installing systemd unit"
install -m 0644 "$SCRIPT_DIR/dstarrepeaterd.service" /etc/systemd/system/dstarrepeaterd.service
systemctl daemon-reload
systemctl enable dstarrepeaterd.service
systemctl restart dstarrepeaterd.service || true

echo
echo "==> Done"
echo "    status:  systemctl status dstarrepeaterd"
echo "    logs:    journalctl -u dstarrepeaterd -f"
