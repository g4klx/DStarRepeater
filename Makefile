export DATADIR := "/usr/share/dstarrepeater"
export LOGDIR  := "/var/log"
export CONFDIR := "/etc"
export BINDIR  := "/usr/bin"

export CXX     := $(shell wx-config --cxx)
export CFLAGS  := -O2 -Wall $(shell wx-config --cxxflags) -DLOG_DIR='$(LOGDIR)' -DCONF_DIR='$(CONFDIR)' -DDATA_DIR='$(DATADIR)'
export GUILIBS := $(shell wx-config --libs adv,core,base) -lasound
export LIBS    := $(shell wx-config --libs base) -lasound -lusb-1.0
export LDFLAGS := 

all:	DStarRepeater/dstarrepeaterd DStarRepeaterConfig/dstarrepeaterconfig

DStarRepeater/dstarrepeaterd:	Common/Common.a force
	$(MAKE) -C DStarRepeater

DStarRepeaterConfig/dstarrepeaterconfig:	GUICommon/GUICommon.a Common/Common.a force
	$(MAKE) -C DStarRepeaterConfig

GUICommon/GUICommon.a: force
	$(MAKE) -C GUICommon

Common/Common.a: force
	$(MAKE) -C Common

.PHONY: install
install:	all
	$(MAKE) -C Data install
	$(MAKE) -C DStarRepeater install
	$(MAKE) -C DStarRepeaterConfig install

.PHONY: clean
clean:
	$(MAKE) -C Common clean
	$(MAKE) -C GUICommon clean
	$(MAKE) -C DStarRepeater clean
	$(MAKE) -C DStarRepeaterConfig clean

.PHONY: force
force:
	@true
