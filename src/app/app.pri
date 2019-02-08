INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/application.h \
    $$PWD/cmdoptions.h \
    $$PWD/filelogger.h \
    $$PWD/qtlocalpeer/qtlocalpeer.h \
    $$PWD/qtlocalpeer/qtlockedfile.h \
    $$PWD/applicationinstancemanager.h \
    $$PWD/upgrade.h

SOURCES += \
    $$PWD/application.cpp \
    $$PWD/cmdoptions.cpp \
    $$PWD/filelogger.cpp \
    $$PWD/main.cpp \
    $$PWD/qtlocalpeer/qtlocalpeer.cpp \
    $$PWD/qtlocalpeer/qtlockedfile.cpp \
    $$PWD/qtlocalpeer/qtlockedfile_unix.cpp \
    $$PWD/qtlocalpeer/qtlockedfile_win.cpp \
    $$PWD/applicationinstancemanager.cpp \
    $$PWD/upgrade.cpp

stacktrace {
    unix {
        HEADERS += $$PWD/stacktrace.h
    }
    else {
        HEADERS += $$PWD/stacktrace_win.h
        !nogui {
            HEADERS += $$PWD/stacktracedialog.h
            FORMS += $$PWD/stacktracedialog.ui
        }
    }
}
