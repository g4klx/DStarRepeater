OBJECTS = DStarRepeaterApp.o DStarRepeaterLogRedirect.o DStarRepeaterRXThread.o DStarRepeaterStatusData.o \
	  DStarRepeaterThread.o DStarRepeaterTRXThread.o DStarRepeaterTXRXThread.o DStarRepeaterTXThread.o

all:		dstarrepeaterd

dstarrepeaterd:	$(OBJECTS)
		$(CXX) $(OBJECTS) ../Common/Common.a $(LDFLAGS) $(LIBS) -o dstarrepeaterd

%.o: %.cpp
		$(CXX) -DwxUSE_GUI=0 $(CFLAGS) -I../Common -c -o $@ $<

install:
		install -g bin -o root -m 0775 dstarrepeaterd $(BINDIR)

clean:
		$(RM) dstarrepeaterd *.o *.d *.bak *~

