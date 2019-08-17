add_library(qbt_rss STATIC
# headers
automatedrssdownloader.h
htmlbrowser.h
rssfeedmodel.h
rssfeedsortmodel.h
rssmodel.h
rsssortmodel.h
rsswidget.h

#sources
automatedrssdownloader.cpp
htmlbrowser.cpp
rssfeedmodel.cpp
rssfeedsortmodel.cpp
rssmodel.cpp
rsssortmodel.cpp
rsswidget.cpp

# forms
automatedrssdownloader.ui
rsswidget.ui
)

target_include_directories(qbt_rss PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qbt_rss
    PRIVATE
        qbt_gui_headers
    PUBLIC
        qbt_base Qt5::Gui Qt5::Widgets Qt5::Network
)
