CC      ?= cc
CFLAGS  := -std=c99 -Wall -Wextra -pedantic -g -I. \
           -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD   := build
TESTBIN := $(BUILD)/test_zodyaml
REGBIN  := $(BUILD)/test_regression

.PHONY: test clean

test: $(TESTBIN) $(REGBIN)
	./$(TESTBIN)
	./$(REGBIN)

$(TESTBIN): test_zodyaml.c zodyaml.c zodyaml.h | $(BUILD)
	$(CC) $(CFLAGS) test_zodyaml.c -o $@

$(REGBIN): test_regression.c zodyaml.h test.yaml | $(BUILD)
	$(CC) $(CFLAGS) test_regression.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
