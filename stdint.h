#pragma once

// Inteiros sem sinal
typedef unsigned char      uint8_t;   // 8 bits  (0 a 255)
typedef unsigned short     uint16_t;  // 16 bits (0 a 65535)
typedef unsigned int       uint32_t;  // 32 bits (0 a ~4 bilhões)
typedef unsigned long long uint64_t;  // 64 bits

// Inteiros com sinal
typedef signed char        int8_t;    // 8 bits  (-128 a 127)
typedef signed short       int16_t;   // 16 bits
typedef signed int         int32_t;   // 32 bits
typedef signed long long   int64_t;   // 64 bits

// Tipos auxiliares úteis em kernels
typedef uint32_t           uintptr_t; // ponteiro como inteiro (32-bit)
typedef int32_t            intptr_t;
typedef uint32_t           size_t;    // tamanho de memória
