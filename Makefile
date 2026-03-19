.PHONY: install installdirs clean force

export BUILD   ?= release
export DATADIR ?= /usr/share/dstarrepeater
export CONFDIR ?= /etc/dstarrepeater
export BINDIR  ?= /usr/bin

# MQTT is on by default. Use MQTT=0 to disable.
MQTT           ?= 1

# GPIO is on by default for ARM Linux (Raspberry Pi). Use GPIO=0 to disable,
# or GPIO=1 to force-enable on other platforms.
ARCH           := $(shell uname -m)
OS             := $(shell uname -s)
ifeq ($(OS),Linux)
    ifneq (,$(filter arm% aarch64,$(ARCH)))
        GPIO   ?= 1
    else
        GPIO   ?= 0
    endif
else
    GPIO       ?= 0
endif

DEBUGFLAGS     := -g -D_DEBUG
RELEASEFLAGS   := -DNDEBUG
export CXX     := g++
export CFLAGS  := -std=c++17 -O2 -Wall
ifeq ($(BUILD), debug)
    export CFLAGS  := $(CFLAGS) $(DEBUGFLAGS)
else ifeq ($(BUILD), release)
    export CFLAGS  := $(CFLAGS) $(RELEASEFLAGS)
endif
export LIBS    := -lpthread -lasound -lusb-1.0
export LDFLAGS :=

ifeq ($(MQTT), 1)
    export CFLAGS := $(CFLAGS) -DMQTT
    export LIBS   := $(LIBS) -lmosquitto
endif

ifeq ($(GPIO), 1)
    export CFLAGS := $(CFLAGS) -DGPIO
    export LIBS   := $(LIBS) -lwiringPi
endif

all: DStarRepeater/dstarrepeaterd

DStarRepeater/dstarrepeaterd: Common/Common.a force
	$(MAKE) -C DStarRepeater

Common/Common.a: force
	$(MAKE) -C Common

installdirs:
	/bin/mkdir -p $(DESTDIR)$(DATADIR) $(DESTDIR)$(CONFDIR) $(DESTDIR)$(BINDIR)

install: all installdirs
	$(MAKE) -C Data install
	$(MAKE) -C DStarRepeater install
	install -g root -o root -m 0644 Data/dstarrepeater.ini.example $(DESTDIR)$(CONFDIR)/dstarrepeater.ini.example

clean:
	$(MAKE) -C Common clean
	$(MAKE) -C DStarRepeater clean

force:
	@true
