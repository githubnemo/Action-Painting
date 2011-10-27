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

USED_LIBS += cv
USED_LIBS += highgui
USED_LIBS += cxcore
INC_DIRS += /usr/include/opencv

include ../NiteSampleMakefile

OUTPUT_DIR = $(shell dirname $(OUTPUT_FILE))
DATA_FOLDER = $(OUTPUT_DIR)/Data
DATA_FILES = background.jpg \
			 Sample-Players.xml

DATA = $(addprefix $(DATA_FOLDER)/,$(DATA_FILES))

$(DATA_FOLDER):
	mkdir $@

$(DATA): $(DATA_FOLDER)

$(DATA_FOLDER)/%: Data/%
	cp $< $@

run: $(OUTPUT_FILE) $(DATA)
	cd $(OUTPUT_DIR) && ./$(shell basename $<)
