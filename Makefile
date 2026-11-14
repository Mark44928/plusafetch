# plusafetch — plain-Make build (CMake remains the supported canonical build;
# this Makefile mirrors the same sources for quick one-shot builds).
# Tools: clang++ (≥17) or g++ with C++23.
# Never -j$(nproc); use make -j2 on RAM-limited devices.

CXX      ?= clang++
CXXFLAGS ?= -O2 -std=c++23 -pthread -Wall -Wextra -Isrc -Ithird_party
LDLIBS   ?= -pthread
PREFIX   ?= $(shell if [ "$$TERMUX_VERSION" != "" ]; then echo "$$PREFIX"; else echo "$$HOME/.local"; fi)

SRC := src/main.cpp \
       src/registry.cpp \
       src/detection/platform.cpp \
       src/detection/platform_android.cpp \
       src/detection/platform_linux.cpp \
       src/detection/common_detect.cpp \
       src/modules/core_modules.cpp \
       src/modules/os.cpp \
       src/modules/cpu.cpp \
       src/modules/memory.cpp \
       src/modules/battery.cpp \
       src/modules/sensors.cpp \
       src/modules/telephony.cpp \
       src/modules/wifi.cpp

BIN      := plusafetch

.PHONY: all clean install run check skins completions-install

all: $(BIN)

$(BIN): $(SRC) $(wildcard src/*.hpp src/*/*.hpp)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f $(BIN)

install: $(BIN)
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 0755 $(BIN) "$(DESTDIR)$(PREFIX)/bin/$(BIN)"
	@echo "installed $(BIN) -> $(DESTDIR)$(PREFIX)/bin/$(BIN)"

run: install
	$(DESTDIR)$(PREFIX)/bin/$(BIN) --version

check: $(BIN)
	./$(BIN) --version
	./$(BIN) --list-modules
	./$(BIN) --json
	./$(BIN) --format '{os} :: {cpu} :: {memory} :: {kernel~0,7}'
	./$(BIN) --verify-config || true

# Validate every shipped ricer skin against the real config parser.
skins: $(BIN)
	@set -e; for f in docs/skins/*.jsonc; do \
		PLUSAFETCH_CONFIG="$$PWD/$$f" ./$(BIN) --verify-config >/dev/null; \
		echo "ok $$f"; \
	done

completions-install:
	install -d "$(DESTDIR)$(PREFIX)/share/bash-completion/completions"
	install -m 0644 completions/plusafetch.bash "$(DESTDIR)$(PREFIX)/share/bash-completion/completions/plusafetch"
	install -d "$(DESTDIR)$(PREFIX)/share/zsh/site-functions"
	install -m 0644 completions/_plusafetch "$(DESTDIR)$(PREFIX)/share/zsh/site-functions/_plusafetch"
