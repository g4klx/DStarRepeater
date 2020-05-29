OBJECTS = AddressTextCtrl.o CallsignTextCtrl.o MessageTextCtrl.o PortTextCtrl.o RestrictedTextCtrl.o

.PHONY: all clean
all:		GUICommon.a

GUICommon.a:	$(OBJECTS)
		$(AR) rcs GUICommon.a $(OBJECTS)

-include $(OBJECTS:.o=.d)

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<
		$(CXX) -MM $(CFLAGS) $< > $*.d

clean:
		$(RM) GUICommon.a *.o *.d *.bak *~

