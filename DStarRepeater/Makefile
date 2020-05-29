OBJECTS = DStarRepeaterApp.o DStarRepeaterLogRedirect.o DStarRepeaterRXThread.o DStarRepeaterStatusData.o \
	  DStarRepeaterThread.o DStarRepeaterTRXThread.o DStarRepeaterTXRXThread.o DStarRepeaterTXThread.o

.PHONY: all install clean

all: dstarrepeaterd

dstarrepeaterd:	$(OBJECTS) ../Common/Common.a
		$(CXX) $(OBJECTS) ../Common/Common.a $(LDFLAGS) $(LIBS) -o dstarrepeaterd

-include $(OBJECTS:.o=.d)

%.o: %.cpp
		$(CXX) -DwxUSE_GUI=0 $(CFLAGS) -I../Common -c -o $@ $<
		$(CXX) -MM -DwxUSE_GUI=0 $(CFLAGS) -I../Common $< > $*.d

install:
		install -g root -o root -m 0755 dstarrepeaterd $(DESTDIR)$(BINDIR)

clean:
		$(RM) dstarrepeaterd *.o *.d *.bak *~

../Common/Common.a:
