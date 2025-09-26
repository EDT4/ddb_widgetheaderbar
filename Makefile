CC?=gcc
CFLAGS+=-O3 -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -flto=auto -std=c99 -D_GNU_SOURCE
LDFLAGS+=-shared -flto
LIBFLAGS=`pkg-config --cflags $(GTKMM) $(GTK)`
LIBS=`pkg-config --libs $(GTKMM) $(GTK)`

SRC_DIR?=src
OUT_DIR?=out

HEADERS=$(shell find src -type f -name '*.h')
SOURCES=$(shell find src -type f -name '*.c')
OBJECTS=$(patsubst $(SRC_DIR)/%.c, $(OUT_DIR)/%.o, $(SOURCES))

define compile
	$(CC) $(CFLAGS) $1 $2 $< -c $(LIBFLAGS) -o $@
endef

define link
	$(CC) $(LDFLAGS) $1 $2 $3 -o $@ $(LIBS)
endef

.PHONY: debug gtk3 gtk2 clean test

debug: GTKMM=gtkmm-3.0
debug: GTK=gtk+-3.0
debug: CFLAGS+= -g
debug: $(OUT_DIR) $(SOURCES) $(OUT_DIR)/customheaderbar_gtk3_debug.so

gtk3: GTKMM=gtkmm-3.0
gtk3: GTK=gtk+-3.0
gtk3: CFLAGS+= -DNDEBUG
gtk3: $(OUT_DIR) $(SOURCES) $(OUT_DIR)/customheaderbar_gtk3.so

$(OUT_DIR):
	@echo "Creating build directories"
	@mkdir -p `find $(SRC_DIR) -type d | sed 's/$(SRC_DIR)/$(OUT_DIR)/'`

$(OUT_DIR)/%.so: $(OBJECTS)
	@echo "Linking"
	@$(call link, $(OBJECTS))
	@echo "Done!"

$(OUT_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling $(subst $(OUT_DIR)/,,$@)"
	@$(call compile)

clean:
	@echo "Cleaning files from previous build..."
	@rm -r -f $(OUT_DIR)
