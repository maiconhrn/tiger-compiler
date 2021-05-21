# tiger-compiler
Implementation of a compiler of Tiger language for Informatics college course at Universidade Estadual de Maringá (UEM).

# Prerequisites
- Linux based system
- g++ 9.3.0>=
- clang++>=10
- QT 5 (qmake)
- flex 2.6.4>=
- bison 3.5.1>=
- LLVM 10 (Maybe you will need to change the value of "LLVM" variable in [tiger-compiler.pro](./tiger-compiler.pro) to a your PC settings compatible value)

# Instructions
The compile phase generated files will be placed in "{{projectRootDir}}/build/" directory.

To compile run:  
- Generate the {{projectRootDir}}/Makefile:
    ```shell
    $ qmake tiger-compiler.pro
    ```
- Compile project to generate the {{projectRootDir}}/build/tc executable:
    ```shell
    $ make
    ```

To execute run:  
```shell
$ ./build/tc -p {{path to the file with Tiger code}} -l{{path to runtime.cpp or runtime.o file}}
```

# Examples
You can use the [test.tig](./test.tig) file for test.

ex:  
```shell
$ ./build/tc -p test.tig -lsrc/utils/runtime.cpp
```   

Compile exec options are:  
 - "-p {{path to the file with Tiger code}}" : specifies the path of the file with Tiger code to be compiled.
 - "-a" : print generated ABS for "-p" file.
 - "-i {{output file}}" : output LLVM IR text representation for "-p" file.
 - "-o {{output file}}" : output the compiled executable for "-p" file.
 - "-l{{lib path}}" : add external lib to be compiled with "-p" file.
 - "-no-codegen" : skips the codegen phase, i.e., execute only Syntactic and Semantic analysis.  
OBS: the use of "-p {{path to the file with Tiger code}}" and "-l{{path to runtime.cpp or runtime.o file}}" options are obligatory.
