#pragma once

// Private shared conversion helpers. SEE: (MISL_FORMAT.md, Source and build)
#if !defined(__riscv) || (defined(__riscv_flen) && __riscv_flen >= 64)
#define MISL_FORMAT_FLOAT 1
#else
#define MISL_FORMAT_FLOAT 0
#endif

const char* misl_format_digits32(char* end, unsigned value, unsigned base, int upper);
const char* misl_format_digits64(char* end, unsigned long long value, unsigned base, int upper);
#if MISL_FORMAT_FLOAT
int misl_format_is_nan(double value);
int misl_format_is_infinite(double value);
int misl_format_sign_bit(double value);
unsigned misl_format_scientific(char* output, double value, int precision, int alternate_form, int upper);
unsigned misl_format_fixed(char* output, double value, int precision, int alternate_form);
unsigned misl_format_general(char* output, double value, int precision, int alternate_form, int upper);
#endif
