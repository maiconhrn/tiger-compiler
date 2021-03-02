######################################################################
# Automatically generated by qmake (3.1) Thu Feb 25 21:31:33 2021
######################################################################

QT       -= gui core

LLVM = llvm-config-10

TEMPLATE = app
TARGET = tc

INCLUDEPATH += $$system($$LLVM --includedir)

INCLUDEPATH += \
    src/

QMAKE_CXXFLAGS += $(shell $$LLVM --cxxflags)

LIBS += $(shell $$LLVM --ldflags --system-libs --libs all)

FLEXSOURCES += src/tiger.l
flex.name = Flex ${QMAKE_FILE_IN}
flex.input = FLEXSOURCES
flex.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.lexer.cpp
flex.commands = flex -o ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.lexer.cpp ${QMAKE_FILE_IN}
flex.CONFIG += target_predeps
flex.variable_out = GENERATED_SOURCES
silent:flex.commands = @echo Lex ${QMAKE_FILE_IN} && $$flex.commands
QMAKE_EXTRA_COMPILERS += flex

BISONSOURCES += src/tiger.y
bison.name = Bison ${QMAKE_FILE_IN}
bison.input = BISONSOURCES
bison.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.parser.cpp
bison.commands = bison -dv -o ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.parser.cpp ${QMAKE_FILE_IN}
bison.CONFIG += target_predeps
bison.variable_out = GENERATED_SOURCES
silent:bison.commands = @echo Bison ${QMAKE_FILE_IN} && $$bison.commands
QMAKE_EXTRA_COMPILERS += bison
bison_header.input = BISONSOURCES
bison_header.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.parser.hpp
bison_header.commands = bison -dv -o ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.parser.cpp ${QMAKE_FILE_IN}
bison_header.CONFIG += target_predeps no_link
silent:bison_header.commands = @echo Bison ${QMAKE_FILE_IN} && $$bison.commands
QMAKE_EXTRA_COMPILERS += bison_header

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Input
HEADERS += src/ast/ast.hpp \
           src/utils/codegencontext.hpp \
           src/utils/symboltable.hpp

SOURCES += src/main.cpp \
           src/ast/ast.cpp \
           src/codegen/codegen.cpp \
           src/utils/codegencontext.cpp \
           src/utils/symboltable.cpp \

OTHER_FILES += \
    src/tiger.l \
    src/tiger.y

DESTDIR = build
OBJECTS_DIR = build
MOC_DIR = build