set(THEME_SOURCES
    Library.cpp
    Library.h
    Options.cpp
    Options.h
    Theme.h
)

if(WIN32)
    list(APPEND THEME_SOURCES Windows/Theme.cpp)
    set(THEME_PLATFORM_LIBRARIES comctl32 uxtheme dwmapi)
elseif(APPLE)
    list(APPEND THEME_SOURCES Mac/Theme.mm)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    set(THEME_PLATFORM_LIBRARIES ${COCOA_FRAMEWORK})
elseif(UNIX)
    list(APPEND THEME_SOURCES Linux/Theme.cpp)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(THEME_GTK REQUIRED IMPORTED_TARGET gtk+-3.0)
    set(THEME_PLATFORM_LIBRARIES PkgConfig::THEME_GTK)
endif()

luwow_add_library(Luwow.Theme
    NAME theme
    MODES SERIAL
    SOURCES ${THEME_SOURCES}
)

target_include_directories(Luwow.Theme PRIVATE
    ${THEME_ROOT}/src
    ${GUI_ROOT}/src
)

target_link_libraries(Luwow.Theme PUBLIC ${THEME_PLATFORM_LIBRARIES})

if(APPLE)
    set_target_properties(Luwow.Theme PROPERTIES OBJCXX_STANDARD 20)
    target_compile_options(Luwow.Theme PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
endif()
