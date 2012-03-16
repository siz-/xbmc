.PHONY: compile

SRCS = AMLPlayer.cpp
SRCS+= FileURLProtocol.cpp

LIB = amlplayer.a

include /opt/pivos/xbmc-aml/Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

