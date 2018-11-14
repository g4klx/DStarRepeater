OBJECTS = AddressTextCtrl.o CallsignTextCtrl.o MessageTextCtrl.o PortTextCtrl.o RestrictedTextCtrl.o

.PHONY: all
all:		GUICommon.a

GUICommon.a:	$(OBJECTS)
		$(AR) rcs GUICommon.a $(OBJECTS)

-include $(OBJECTS:.o=.d)

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<
		$(CXX) -MM $(CFLAGS) $< > $*.d

.PHONY: clean
clean:
		$(RM) GUICommon.a *.o *.d *.bak *~

