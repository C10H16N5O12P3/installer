
BUILD := build

TARGET := BeyondStyxInstaller.exe
OUT := out

CXX := x86_64-w64-mingw32-g++
LD := x86_64-w64-mingw32-g++
UIC := uic
MOC := moc
RCC := rcc

QT_PATH := C:/Qt/5.15.2/mingw81_64

DEPLOY := windeployqt.exe

CXX_FLAGS := -fpermissive -mwindows -O3

LD_FLAGS := -mwindows

INCDIRS := $(addprefix -I, \
	$(BUILD) \
	$(QT_PATH)/include \
	$(QT_PATH)/include/QtCore \
	$(QT_PATH)/include/QtGui \
	$(QT_PATH)/include/QtWidgets \
)

LIBDIRS := $(addprefix -L, \
	$(QT_PATH)/lib \
)

LIBS := $(addprefix -l, \
	Qt5Widgets \
	Qt5Gui \
	Qt5Core \
	Qt5Network \
)

OBJS := $(addprefix $(BUILD)/, \
	main.o \
	3rdParty/zip.o \
	mainwindow.o \
	qrc_resources.o \
	moc_mainwindow.o \
)

RCC_SRC := $(addsuffix .cpp, $(addprefix $(BUILD)/qrc_, \
	resources \
))

WINS := \
	mainwindow

UIC_SRC := $(addsuffix .h, $(addprefix $(BUILD)/ui_, $(WINS)))
MOC_SRC := $(addsuffix .cpp, $(addprefix $(BUILD)/moc_, $(WINS)))

QT_OBJ := $(WINS)

$(shell mkdir -p $(addprefix $(BUILD)/, . 3rdParty) $(OUT))


all: moc $(OUT)/$(TARGET)

moc: $(UIC_SRC) $(MOC_SRC) $(RCC_SRC) 
	
clean:
	rm -rf build $(OUT)/$(TARGET)

$(BUILD)/ui_%.h : %.ui
	$(UIC) $< -o $@

$(BUILD)/moc_%.cpp : %.h
	$(MOC) $< -o $@

$(BUILD)/qrc_%.cpp : %.qrc
	$(RCC) $< -o $@

$(BUILD)/%.o : $(BUILD)/%.cpp
	$(CXX) $(CXX_FLAGS) $(INCDIRS) -c $< -o $@

$(BUILD)/%.o : %.cpp
	$(CXX) $(CXX_FLAGS) $(INCDIRS) -c $< -o $@

$(OUT)/$(TARGET) : $(OBJS)
	$(LD) $^ $(LIBDIRS) $(LIBS) $(LD_FLAGS) -o $@
	$(DEPLOY) $@


	
.PHONY: all clean moc


