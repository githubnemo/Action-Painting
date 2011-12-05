OSTYPE := $(shell uname -s)

SRC_FILES = \
	main.cpp \
	SceneDrawer.cpp \
	SmudgeEffect/Brush.cpp \
	SmudgeEffect/smudge_util.cpp

INC_DIRS += ../../../../../Samples/MyPlayers
INC_DIRS += ../../../../../Samples/MyPlayers/SmudgeEffect

EXE_NAME = Sample-MyPlayers

DEFINES = USE_GLUT

ifeq ("$(OSTYPE)","Darwin")
        LDFLAGS += -framework OpenGL -framework GLUT
		USED_LIBS += opencv_core
		USED_LIBS += opencv_highgui
		USED_LIBS += opencv_imgproc
#		USED_LIBS += cxcore
		INC_DIRS += /usr/local/include/opencv

else
		USED_LIBS += glut
		USED_LIBS += cv
		USED_LIBS += highgui
		USED_LIBS += cxcore
		INC_DIRS += /usr/include/opencv
endif


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

debug: $(OUTPUT_FILE) $(DATA)
	cd $(OUTPUT_DIR) && gdb ./$(shell basename $<)
