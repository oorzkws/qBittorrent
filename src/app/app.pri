INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/applicationimpl.h \
    $$PWD/upgrade.h

SOURCES += \
    $$PWD/applicationimpl.cpp \
    $$PWD/main.cpp \
    $$PWD/upgrade.cpp

stacktrace {
    unix {
        HEADERS += $$PWD/stacktrace.h
    }
    else {
        HEADERS += $$PWD/stacktrace_win.h
        !nogui {
            HEADERS += $$PWD/stacktracedialog.h
            SOURCES += $$PWD/stacktracedialog.cpp
            FORMS += $$PWD/stacktracedialog.ui
        }
    }
}
