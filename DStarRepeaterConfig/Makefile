OBJECTS = DStarRepeaterConfigActiveHangSet.o DStarRepeaterConfigAnnouncementSet.o DStarRepeaterConfigApp.o DStarRepeaterConfigBeaconSet.o \
	  DStarRepeaterConfigCallsignSet.o DStarRepeaterConfigControl1Set.o DStarRepeaterConfigControl2Set.o \
	  DStarRepeaterConfigControllerSet.o DStarRepeaterConfigDVAPSet.o DStarRepeaterConfigDVMegaSet.o DStarRepeaterConfigDVRPTR1Set.o \
	  DStarRepeaterConfigDVRPTR2Set.o DStarRepeaterConfigDVRPTR3Set.o DStarRepeaterConfigFrame.o DStarRepeaterConfigGMSKSet.o \
	  DStarRepeaterConfigIcomSet.o DStarRepeaterConfigMMDVMSet.o DStarRepeaterConfigModemSet.o DStarRepeaterConfigNetworkSet.o \
	  DStarRepeaterConfigSoundCardSet.o DStarRepeaterConfigSplitSet.o DStarRepeaterConfigTimesSet.o

.PHONY: all install clean
all: dstarrepeaterconfig

dstarrepeaterconfig:	$(OBJECTS) ../GUICommon/GUICommon.a ../Common/Common.a
		$(CXX) $(OBJECTS) ../GUICommon/GUICommon.a ../Common/Common.a $(LDFLAGS) $(GUILIBS) -o dstarrepeaterconfig

-include $(OBJECTS:.o=.d)

%.o: %.cpp
		$(CXX) $(CFLAGS) -I../GUICommon -I../Common -c -o $@ $<
		$(CXX) -MM $(CFLAGS) -I../GUICommon -I../Common $< > $*.d

install:
		install -g root -o root -m 0755 dstarrepeaterconfig $(DESTDIR)$(BINDIR)

clean:
		$(RM) dstarrepeaterconfig *.o *.d *.bak *~

../GUICommon/GUICommon.a:
../Common/Common.a:
