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
$ ./build/tc -p {{path to the file with Tiger code}}
```

# Examples
You can use the [test.tig](./test.tig) file for test.

ex:  
```shell
$ ./build/tc -p test.tig
```
