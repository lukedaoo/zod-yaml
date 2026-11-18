CC      ?= cc
CFLAGS  := -std=c99 -Wall -Wextra -pedantic -g -I. \
           -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD   := build
TESTBIN := $(BUILD)/test_zodyaml
REGBIN  := $(BUILD)/test_regression
CXX     ?= c++
CXXFLAGS := -std=c++11 -Wall -Wextra -pedantic -g -I. \
            -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: test examples clean

test: $(TESTBIN) $(REGBIN)
	./$(TESTBIN)
	./$(REGBIN)

$(TESTBIN): test_zodyaml.c zodyaml.c zodyaml.h | $(BUILD)
	$(CC) $(CFLAGS) test_zodyaml.c -o $@

$(REGBIN): test_regression.c zodyaml.h test.yaml | $(BUILD)
	$(CC) $(CFLAGS) test_regression.c -o $@

examples: $(BUILD)/example $(BUILD)/example_cpp
	./$(BUILD)/example
	./$(BUILD)/example_cpp

$(BUILD)/zodyaml.o: zodyaml.c zodyaml.h | $(BUILD)
	$(CC) $(CFLAGS) -c zodyaml.c -o $@

$(BUILD)/example: example.c $(BUILD)/zodyaml.o
	$(CC) $(CFLAGS) example.c $(BUILD)/zodyaml.o -o $@

$(BUILD)/example_cpp: example.cpp zodyaml.hpp $(BUILD)/zodyaml.o
	$(CXX) $(CXXFLAGS) example.cpp $(BUILD)/zodyaml.o -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
