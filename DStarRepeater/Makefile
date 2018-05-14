OBJECTS = DStarRepeaterApp.o DStarRepeaterFrame.o DStarRepeaterLogger.o DStarRepeaterRXThread.o DStarRepeaterStatusData.o \
	  DStarRepeaterThread.o DStarRepeaterTRXThread.o DStarRepeaterTXRXThread.o DStarRepeaterTXThread.o

all:		dstarrepeater

dstarrepeater:	$(OBJECTS)
		$(CXX) $(OBJECTS) ../Common/Common.a $(LDFLAGS) $(LIBS) -o dstarrepeater

%.o: %.cpp
		$(CXX) $(CFLAGS) -I../Common -c -o $@ $<

clean:
		$(RM) dstarrepeater *.o *.d *.bak *~

