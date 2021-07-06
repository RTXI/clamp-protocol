PLUGIN_NAME = clamp_protocol

RTXI_INCLUDES =

HEADERS = clamp-protocol.h\
          protocol.h\
          clamp-protocol-editor.h\
          clamp-protocol-window.h\

SOURCES = clamp-protocol.cpp \
          moc_clamp-protocol.cpp\
          protocol.cpp\
          clamp-protocol-editor.cpp\
          moc_clamp-protocol-editor.cpp\
          clamp-protocol-window.cpp\
          moc_clamp-protocol-window.cpp\

LIBS = -lqwt-qt5 -lrtplot

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
