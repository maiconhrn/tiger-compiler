PROJECT_NAME=tiger-compiler
SRC_DIR=src
BUILD_DIR=build
RM=rm -r -f
MKDIR=mkdir -p
SRCS=$(wildcard $(SRC_DIR)/*.cpppp)
OBJS=$(SRCS:$(SRC_DIR)%.cpppp=$(BUILD_DIR)%.o)
BIN=$(BUILD_DIR)/$(PROJECT_NAME)
SHELL=/bin/bash

.PHONY: clean

all: $(PROJECT_NAME)

$(PROJECT_NAME): mkdir-build $(BUILD_DIR)/driver.o $(BUILD_DIR)/lex.yy.o
	g++ -g -o $(BIN) $(BUILD_DIR)/driver.o $(BUILD_DIR)/lex.yy.o

$(BUILD_DIR)/driver.o: $(SRC_DIR)/driver.cpp $(SRC_DIR)/tokens.hpp
	g++ -g -c $(SRC_DIR)/driver.cpp -o $(BUILD_DIR)/driver.o

$(BUILD_DIR)/lex.yy.o: $(SRC_DIR)/lex.yy.cpp $(SRC_DIR)/tokens.hpp
	g++ -g -c $(SRC_DIR)/lex.yy.cpp -o $(BUILD_DIR)/lex.yy.o

$(SRC_DIR)/lex.yy.cpp: $(SRC_DIR)/tiger.lex
	lex -o $(SRC_DIR)/lex.yy.cpp $(SRC_DIR)/tiger.lex

mkdir-build:
	$(MKDIR) $(BUILD_DIR)

clean: 
	$(RM) $(BUILD_DIR) $(SRC_DIR)/lex.yy.cpp
