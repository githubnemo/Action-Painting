OSTYPE := $(shell uname -s)

SRC_FILES = \
	main.cpp \
	SceneDrawer.cpp \
	kbhit.cpp \
	signal_catch.cpp

INC_DIRS += ../../../../../Samples/MyPlayers

EXE_NAME = Sample-MyPlayers

DEFINES = USE_GLUT

ifeq ("$(OSTYPE)","Darwin")
        LDFLAGS += -framework OpenGL -framework GLUT
else
        USED_LIBS += glut
endif

include ../NiteSampleMakefile

