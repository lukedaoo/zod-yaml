CC      ?= cc
CFLAGS  := -std=c99 -Wall -Wextra -pedantic -g -I. \
           -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD   := build
TESTBIN := $(BUILD)/test_zodyaml

.PHONY: test clean

test: $(TESTBIN)
	./$(TESTBIN)

$(TESTBIN): test_zodyaml.c zodyaml.c zodyaml.h | $(BUILD)
	$(CC) $(CFLAGS) test_zodyaml.c zodyaml.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
