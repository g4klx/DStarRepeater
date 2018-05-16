export DATADIR := "/usr/share/dstarrepeater"
export LOGDIR  := "/var/log"
export CONFDIR := "/etc"
export BINDIR  := "/usr/bin"

export CXX     := $(shell wx-config --cxx)
export CFLAGS  := -O2 -Wall $(shell wx-config --cxxflags) -DLOG_DIR='$(LOGDIR)' -DCONF_DIR='$(CONFDIR)' -DDATA_DIR='$(DATADIR)'
export GUILIBS := $(shell wx-config --libs adv,core,base) -lasound
export LIBS    := $(shell wx-config --libs base) -lasound -lusb-1.0
export LDFLAGS := 

all:	DStarRepeater/dstarrepeater DStarRepeaterConfig/dstarrepeaterconfig

DStarRepeater/dstarrepeater:	Common/Common.a
	make -C DStarRepeater dstarrepeater

DStarRepeaterConfig/dstarrepeaterconfig:	GUICommon/GUICommon.a Common/Common.a
	make -C DStarRepeaterConfig

GUICommon/GUICommon.a:
	make -C GUICommon

Common/Common.a:
	make -C Common

install:	all
	make -C Data install
	make -C DStarRepeater install
	make -C DStarRepeaterConfig install

clean:
	make -C Common clean
	make -C GUICommon clean
	make -C DStarRepeater clean
	make -C DStarRepeaterConfig clean

