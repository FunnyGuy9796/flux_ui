CC = cc
PKGCONF = pkg-config
PKGS = gbm egl glesv2
SRC_DIR = src
API_DIR = api
OBJ_DIR = obj
TARGET = compositor
API_TARGET = flux_api.o

SRCS := $(shell find $(SRC_DIR) -name '*.c')
API_SRCS := $(shell find $(API_DIR) -name '*.c')

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
API_OBJS := $(patsubst $(API_DIR)/%.c,$(API_DIR)/%.o,$(API_SRCS))

CFLAGS = -Wall -O2 -I/usr/local/include -I/usr/local/include/libdrm $(shell $(PKGCONF) --cflags $(PKGS))
LDFLAGS = -L/usr/local/lib $(shell $(PKGCONF) --libs $(PKGS)) -ldrm -lm

all: $(TARGET)
api: flux_api.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(API_TARGET): $(API_OBJS)
	$(CC) -c -o $@ $^

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

clean-api:
	rm -rf $(API_DIR)/$(API_TARGET)

run:
	./$(TARGET)

.PHONY: all clean run