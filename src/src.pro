# //////////////////////////////////////////////////
# //                                              //
# // Emu64                                        //
# // von Thorsten Kattanek                        //
# //                                              //
# // #file: scr.pro                               //
# //                                              //
# // Dieser Sourcecode ist Copyright geschützt!   //
# // Geistiges Eigentum von Th.Kattanek           //
# //                                              //
# // Letzte Änderung am 01.09.2019                //
# // www.emu64.de                                 //
# //                                              //
# //////////////////////////////////////////////////

# PREFIX=/usr/local

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4){

QT += widgets

TARGET = emu64
TEMPLATE = app

CONFIG += c++11

# Versionsnummer ermitteln aus Git Tag Nummer
GIT_VERSION = $$system(git --git-dir \"../.git\" describe --always --tags)
DEFINES += VERSION_STRING=\\\"$$GIT_VERSION\\\"

message("Emu64 Version: " $$GIT_VERSION)

# Linux Architecture
linux-g++{
   !contains(QT_ARCH, x86_64){
       DEFINES += ARCHITECTURE_STRING=\\\"32Bit\\\"
       message("Compiling for Linux 32bit system")
    } else {
       DEFINES += ARCHITECTURE_STRING=\\\"64Bit\\\"
       message("Compiling for Linux 64bit system")
   }
}

# Windows Architecture
win32{
   !contains(QT_ARCH, x86_64){
       DEFINES += ARCHITECTURE_STRING=\\\"32Bit\\\"
       message("Compiling for Windows 32bit system")
    } else {
       DEFINES += ARCHITECTURE_STRING=\\\"64Bit\\\"
       message("Compiling for Windows 64bit system")
   }
}

# Abhängigkeiten

CONFIG += link_pkgconfig
PKGCONFIG += sdl2 SDL2_image libpng glu libavutil libavformat libavcodec libswresample libswscale

# Für Win32 / Libs ohne PKCONFIG angeben
#LIBS += -lsdl2 -lsdl2main -lsdl2_image -lpng -lglu32 -lopengl32 -lavutil -lavformat -lavcodec -lswresample -lswscale

message("Zip: $$ZIP")

linux-g++{
    DEFINES += ZIP_SUPPORT=true
    LIBS += -lquazip5
}

win32{
    DEFINES += ZIP_SUPPORT=true
    PKGCONFIG += quazip
}

# Quelltexte

SOURCES += \
    am29f040_class.cpp \
    button_mod.cpp \
    c64_class.cpp \
    c64_keyboard_window.cpp \
    c64_speed_window.cpp \
    command_line_class.cpp \
    cartridge_class.cpp \
    cartridge_new_easyflash_window.cpp \
    cartridge_window.cpp \
    custom_splashscreen.cpp \
    d64_class.cpp \
    debugger_iec_window.cpp \
    debugger_vic_window.cpp \
    debugger_window.cpp \
    floppy1541_class.cpp \
    floppy_new_d64_window.cpp \
    floppy_window.cpp \
    georam_class.cpp \
    info_window.cpp \
    label_widget_mod.cpp \
    main.cpp \
    main_window.cpp \
    memory_window.cpp \
    mmu_class.cpp \
    mos6502_class.cpp \
    mos6510_class.cpp \
    mos6522_class.cpp \
    mos6526_class.cpp \
    mos6526_port.cpp \
    mos6569_class.cpp \
    mos6581_8085_class.cpp \
    new_romset_window.cpp \
    reu_class.cpp \
    savepng.c \
    setup_window.cpp \
    show_c64_key_mapping_window.cpp \
    sid_dump_window.cpp \
    siddump.cpp \
    single_application.cpp \
    tape1530_class.cpp \
    tape_new_window.cpp \
    tape_window.cpp \
    tree_widget_mod.cpp \
    video_crt_setup_window.cpp \
    vcd_class.cpp \
    video_capture_class.cpp \
    video_capture_window.cpp \
    video_crt_class.cpp \
    widget_bit_leiste.cpp \
    widget_file_browse.cpp \
    widget_floppy_status.cpp \
    widget_memory_zeile.cpp \
    widget_sr_edit.cpp \
    widget_tape_counter.cpp

HEADERS += \
    am29f040_class.h \
    button_mod.h \
    c64_class.h \
    c64_keyboard_window.h \
    c64_speed_window.h \
    command_line_class.h \
    cartridge_class.h \
    cartridge_new_easyflash_window.h \
    cartridge_window.h \
    custom_save_file_dialog.h \
    custom_splashscreen.h \
    d64_class.h \
    debugger_iec_window.h \
    debugger_vic_window.h \
    debugger_window.h \
    floppy1541_class.h \
    floppy_new_d64_window.h \
    floppy_window.h \
    georam_class.h \
    info_window.h \
    label_widget_mod.h \
    main_window.h \
    memory_window.h \
    mmu_class.h \
    mos6502_class.h \
    mos6510_class.h \
    mos6522_class.h \
    mos6526_class.h \
    mos6526_port.h \
    mos6569_class.h \
    mos6581_8085_class.h \
    new_romset_window.h \
    reu_class.h \
    savepng.h \
    setup_window.h \
    show_c64_key_mapping_window.h \
    sid_dump_window.h \
    siddump.h \
    single_application.h \
    structs.h \
    tape1530_class.h \
    tape_new_window.h \
    tape_window.h \
    tree_widget_mod.h \
    video_crt_setup_window.h \
    vcd_class.h \
    video_capture_class.h \
    video_capture_window.h \
    video_crt_class.h \
    widget_bit_leiste.h \
    widget_file_browse.h \
    widget_floppy_status.h \
    widget_memory_zeile.h \
    widget_sr_edit.h \
    widget_tape_counter.h

FORMS += \
    c64_keyboard_window.ui \
    c64_speed_window.ui \
    cartridge_new_easyflash_window.ui \
    cartridge_window.ui \
    debugger_iec_window.ui \
    debugger_vic_window.ui \
    debugger_window.ui \
    floppy_new_d64_window.ui \
    floppy_window.ui \
    info_window.ui \
    main_window.ui \
    memory_window.ui \
    new_romset_window.ui \
    setup_window.ui \
    show_c64_key_mapping_window.ui \
    sid_dump_window.ui \
    tape_new_window.ui \
    tape_window.ui \
    video_crt_setup_window.ui \
    video_capture_window.ui \
    widget_bit_leiste.ui \
    widget_file_browse.ui \
    widget_floppy_status.ui \
    widget_memory_zeile.ui \
    widget_sr_edit.ui \
    widget_tape_counter.ui

TRANSLATIONS += emu64_en.ts \
                emu64_de.ts

RESOURCES += emu64.qrc
RC_FILE += emu64.rc

# Installation

message(Installpath: $$PREFIX)
DEFINES += DATA_PATH=\\\"$$PREFIX\\\"

win32{
    target.path = $$PREFIX
    roms.path = $$PREFIX/roms
    floppy_sounds.path = $$PREFIX/floppy_sounds
    gfx.path = $$PREFIX/gfx
    txt.path = $$PREFIX
    languages.path = $$PREFIX/languages
}

linux-g++{
    target.path = $$PREFIX/bin
    roms.path = $$PREFIX/share/$$TARGET/roms
    floppy_sounds.path = $$PREFIX/share/$$TARGET/floppy_sounds
    gfx.path = $$PREFIX/share/$$TARGET/gfx
    txt.path = $$PREFIX/share/$$TARGET
    languages.path = $$PREFIX/share/$$TARGET/languages
}

# C64 Roms
roms.files += ../roms/*

# Floppy Sounds
floppy_sounds.files = ../floppy_sounds/*

# GFX
gfx.files += ../grafik/kreis0.png
gfx.files += ../grafik/kreis1.png
gfx.files += ../grafik/pfeil0.png
gfx.files += ../grafik/pfeil1.png
gfx.files += ../grafik/sdl_icon.png

# TXT
txt.files += ../kommandozeilenparameter.txt
txt.files += ../LICENSE

# Languages
languages.files += ../grafik/flaggen/emu64_de.png
languages.files += ../grafik/flaggen/emu64_en.png
languages.files += emu64_de.qm
languages.files += emu64_en.qm

INSTALLS += target roms floppy_sounds gfx txt languages
}
