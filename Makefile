BUILD := build_rel
BIN   := $(BUILD)/pulse

.PHONY: build run toggle clean \
        mock mock-idle mock-working mock-permission mock-question mock-plan mock-expanded

build:
	cmake --build $(BUILD) --target pulse

run: build
	$(BIN)

toggle:
	cmake --build $(BUILD) --target pulse-toggle

clean:
	cmake --build $(BUILD) --target clean

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
