PROTO_DIR = ../../protocols
SCANNER = wayland-scanner
BUILD_DIR = build

CFLAGS = -Wall -Wextra $(shell pkg-config --cflags wayland-client)
LDFLAGS = $(shell pkg-config --libs wayland-client)

TARGETS = test_blur_full test_blur_partial test_blur_dynamic test_blur_remove test_blur_window

GENERATED = \
	$(BUILD_DIR)/ext-background-effect-v1-client-protocol.h \
	$(BUILD_DIR)/ext-background-effect-v1-protocol.c \
	$(BUILD_DIR)/wlr-layer-shell-unstable-v1-client-protocol.h \
	$(BUILD_DIR)/wlr-layer-shell-unstable-v1-protocol.c \
	$(BUILD_DIR)/xdg-shell-client-protocol.h \
	$(BUILD_DIR)/xdg-shell-protocol.c

PROTO_OBJS = $(BUILD_DIR)/ext-background-effect-v1-protocol.o $(BUILD_DIR)/wlr-layer-shell-unstable-v1-protocol.o $(BUILD_DIR)/xdg-shell-protocol.o

all: $(addprefix $(BUILD_DIR)/,$(TARGETS))

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/ext-background-effect-v1-client-protocol.h: $(PROTO_DIR)/ext-background-effect-v1.xml | $(BUILD_DIR)
	$(SCANNER) client-header $< $@

$(BUILD_DIR)/ext-background-effect-v1-protocol.c: $(PROTO_DIR)/ext-background-effect-v1.xml | $(BUILD_DIR)
	$(SCANNER) private-code $< $@

$(BUILD_DIR)/wlr-layer-shell-unstable-v1-client-protocol.h: $(PROTO_DIR)/wlr-layer-shell-unstable-v1.xml | $(BUILD_DIR)
	$(SCANNER) client-header $< $@

$(BUILD_DIR)/wlr-layer-shell-unstable-v1-protocol.c: $(PROTO_DIR)/wlr-layer-shell-unstable-v1.xml | $(BUILD_DIR)
	$(SCANNER) private-code $< $@

$(BUILD_DIR)/xdg-shell-client-protocol.h: | $(BUILD_DIR)
	$(SCANNER) client-header $$(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml $@

$(BUILD_DIR)/xdg-shell-protocol.c: | $(BUILD_DIR)
	$(SCANNER) private-code $$(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c $(GENERATED) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -c $< -o $@

$(BUILD_DIR)/test_%: test_%.c $(PROTO_OBJS) $(GENERATED) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -o $@ $< $(PROTO_OBJS) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
