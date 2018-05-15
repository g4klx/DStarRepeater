OBJECTS = DStarRepeaterConfigActiveHangSet.o DStarRepeaterConfigAnnouncementSet.o DStarRepeaterConfigApp.o DStarRepeaterConfigBeaconSet.o \
	  DStarRepeaterConfigCallsignSet.o DStarRepeaterConfigControl1Set.o DStarRepeaterConfigControl2Set.o \
	  DStarRepeaterConfigControllerSet.o DStarRepeaterConfigDVAPSet.o DStarRepeaterConfigDVMegaSet.o DStarRepeaterConfigDVRPTR1Set.o \
	  DStarRepeaterConfigDVRPTR2Set.o DStarRepeaterConfigDVRPTR3Set.o DStarRepeaterConfigFrame.o DStarRepeaterConfigGMSKSet.o \
	  DStarRepeaterConfigMMDVMSet.o DStarRepeaterConfigModemSet.o DStarRepeaterConfigNetworkSet.o DStarRepeaterConfigSoundCardSet.o \
	  DStarRepeaterConfigSplitSet.o DStarRepeaterConfigTimesSet.o

all:		dstarrepeaterconfig

dstarrepeaterconfig:	$(OBJECTS)
		$(CXX) $(OBJECTS) ../GUICommon/GUICommon.a ../Common/Common.a $(LDFLAGS) $(LIBS) -o dstarrepeaterconfig

%.o: %.cpp
		$(CXX) $(CFLAGS) -I../GUICommon -I../Common -c -o $@ $<

install:
		install -g bin -o root -m 0775 dstarrepeaterconfig $(BINDIR)

clean:
		$(RM) dstarrepeaterconfig *.o *.d *.bak *~

