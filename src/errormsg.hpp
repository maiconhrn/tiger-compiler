#ifndef TIGER_COMPILER_HPP
#define TIGER_COMPILER_HPP

extern bool EM_anyErrors;

void EM_newline(void);

extern int EM_tokPos;

void EM_error(int, string,...);
void EM_impossible(string,...);
void EM_reset(string filename);

#endif // TIGER_COMPILER_HPP
