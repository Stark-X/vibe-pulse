# ── Platform detection ─────────────────────────────────────────────────────────
UNAME := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME),Darwin)
    BUILD   := build_mac
    BIN     := $(BUILD)/pulse
    TOGGLE  := $(BUILD)/pulse-toggle
else ifeq ($(UNAME),Linux)
    BUILD   := build
    BIN     := $(BUILD)/pulse
    TOGGLE  := $(BUILD)/pulse-toggle
else
    BUILD   := build_win
    BIN     := $(BUILD)/Release/pulse.exe
    TOGGLE  := $(BUILD)/Release/pulse-toggle.exe
endif

.PHONY: build run toggle clean configure \
        mock mock-idle mock-working mock-permission mock-question mock-plan mock-expanded \
        mock-subscription

# Auto-configure if build directory is missing
$(BUILD)/CMakeCache.txt:
	cmake -B $(BUILD) -S . -DCMAKE_BUILD_TYPE=Release

configure: $(BUILD)/CMakeCache.txt

build: $(BUILD)/CMakeCache.txt
	cmake --build $(BUILD) --target pulse

run: build
	$(BIN)

toggle: $(BUILD)/CMakeCache.txt
	cmake --build $(BUILD) --target pulse-toggle
	$(TOGGLE) toggle

clean:
	rm -rf $(BUILD)

# ── Mock preview targets ───────────────────────────────────────────────────────
# 用法: make mock SCENE=<场景>  或直接 make mock-<场景>
# 场景: idle | working | permission | question | plan | expanded

mock: build
	@if [ -z "$(SCENE)" ]; then \
		echo "用法: make mock SCENE=<场景>"; \
		echo "可用场景: idle  working  permission  question  plan  expanded"; \
		exit 1; \
	fi
	PULSE_MOCK=$(SCENE) $(BIN)

mock-idle: build
	PULSE_MOCK=idle $(BIN)

mock-working: build
	PULSE_MOCK=working $(BIN)

mock-permission: build
	PULSE_MOCK=permission $(BIN)

mock-question: build
	PULSE_MOCK=question $(BIN)

mock-plan: build
	PULSE_MOCK=plan $(BIN)

mock-expanded: build
	PULSE_MOCK=expanded $(BIN)

mock-subscription: build
	PULSE_MOCK=working PULSE_MOCK_SUBSCRIPTION=CC=45,CD=31 $(BIN)
