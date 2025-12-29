cmake_minimum_required(VERSION 3.10)
project(webview_gtk_embed_plugin LANGUAGES CXX)

set(PLUGIN_NAME "webview_gtk_embed_plugin")

add_library(${PLUGIN_NAME} SHARED
  "webview_gtk_embed_plugin.cc"
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
)

apply_standard_settings(${PLUGIN_NAME})

set_target_properties(${PLUGIN_NAME} PROPERTIES
  OUTPUT_NAME "webview_gtk_embed_plugin"
)

target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)
pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.1)

target_include_directories(${PLUGIN_NAME} PRIVATE ${GTK_INCLUDE_DIRS} ${WEBKIT2_INCLUDE_DIRS})
target_compile_options(${PLUGIN_NAME} PRIVATE ${GTK_CFLAGS_OTHER} ${WEBKIT2_CFLAGS_OTHER})

target_link_libraries(${PLUGIN_NAME} PRIVATE
  flutter
  ${GTK_LIBRARIES}
  ${WEBKIT2_LIBRARIES}
)

target_link_directories(${PLUGIN_NAME} PRIVATE ${GTK_LIBRARY_DIRS} ${WEBKIT2_LIBRARY_DIRS})
