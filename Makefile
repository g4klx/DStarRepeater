export DATADIR := "/usr/local/etc"
export LOGDIR  := "/var/log"
export CONFDIR := "/etc"

export CXX := $(shell wx-config --cxx)
export CFLAGS := -g -O2 -Wall $(shell wx-config --cxxflags) -DLOG_DIR='$(LOGDIR)' -DCONF_DIR='$(CONFDIR)' -DDATA_DIR='$(DATADIR)'
export LIBS := $(shell wx-config --libs adv,core) -lasound -lusb-1.0
export LDFLAGS := -g

all:	DStarRepeater/dstarrepeater DStarRepeaterConfig/dstarrepeaterconfig

DStarRepeater/dstarrepeater:	Common/Common.a
	make -C DStarRepeater dstarrepeater

DStarRepeaterConfig/dstarrepeaterconfig:	GUICommon/GUICommon.a Common/Common.a
	make -C DStarRepeaterConfig

GUICommon/GUICommon.a:
	make -C GUICommon

Common/Common.a:
	make -C Common

clean:
	make -C Common clean
	make -C GUICommon clean
	make -C DStarRepeater clean
	make -C DStarRepeaterConfig clean

