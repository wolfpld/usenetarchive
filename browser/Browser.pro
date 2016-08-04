#-------------------------------------------------
#
# Project created by QtCreator 2016-07-10T20:11:47
#
#-------------------------------------------------

INCLUDEPATH += $$PWD/../contrib/zstd/common $$PWD/../contrib/zstd

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Browser
TEMPLATE = app


SOURCES += main.cpp\
        browser.cpp \
    ../common/mmap.cpp \
    ../libuat/Archive.cpp \
    ../contrib/zstd/common/entropy_common.c \
    ../contrib/zstd/common/fse_decompress.c \
    ../contrib/zstd/common/xxhash.c \
    ../contrib/zstd/common/zstd_common.c \
    ../contrib/zstd/decompress/huf_decompress.c \
    ../contrib/zstd/decompress/zstd_decompress.c \
    treemodel.cpp \
    treeitem.cpp \
    TextBuf.cpp \
    ../common/LexiconTypes.cpp \
    about.cpp \
    groupcharter.cpp \
    ../libuat/PackageAccess.cpp

HEADERS  += browser.h \
    ../common/MetaView.hpp \
    ../common/mmap.hpp \
    ../libuat/Archive.hpp \
    ../libuat/ViewReference.hpp \
    ../contrib/zstd/common/bitstream.h \
    ../contrib/zstd/common/error_private.h \
    ../contrib/zstd/common/error_public.h \
    ../contrib/zstd/common/fse.h \
    ../contrib/zstd/common/huf.h \
    ../contrib/zstd/common/mem.h \
    ../contrib/zstd/common/xxhash.h \
    ../contrib/zstd/common/zbuff.h \
    ../contrib/zstd/zstd.h \
    ../contrib/zstd/common/zstd_internal.h \
    treemodel.hpp \
    treeitem.hpp \
    TextBuf.hpp \
    ../common/LexiconTypes.hpp \
    about.h \
    groupcharter.h \
    ../libuat/PackageAccess.hpp

FORMS    += browser.ui \
    about.ui \
    groupcharter.ui

DISTFILES += \
    build/win32/x64/Debug/browser.ilk \
    build/win32/x64/Debug/vc140.idb \
    build/win32/x64/Debug/browser.pdb \
    build/win32/x64/Debug/vc140.pdb \
    build/win32/x64/Debug/browser.exe \
    build/win32/browser.VC.db \
    build/win32/x64/Debug/browser.tlog/browser.lastbuildstate \
    build/win32/x64/Debug/browser.tlog/CL.command.1.tlog \
    build/win32/x64/Debug/browser.tlog/CL.read.1.tlog \
    build/win32/x64/Debug/browser.tlog/CL.write.1.tlog \
    build/win32/x64/Debug/browser.tlog/link.command.1.tlog \
    build/win32/x64/Debug/browser.tlog/link.read.1.tlog \
    build/win32/x64/Debug/browser.tlog/link.write.1.tlog \
    build/win32/x64/Debug/browser.log

win32: RC_ICONS = icon.ico
win32: QMAKE_LFLAGS += /STACK:20000000
