OBJECTS = AddressTextCtrl.o CallsignTextCtrl.o MessageTextCtrl.o PortTextCtrl.o RestrictedTextCtrl.o

all:		GUICommon.a

GUICommon.a:	$(OBJECTS)
		$(AR) rcs GUICommon.a $(OBJECTS)

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

clean:
		$(RM) GUICommon.a *.o *.d *.bak *~

