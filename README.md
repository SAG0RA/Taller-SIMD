# Taller 02 – Conversión de Texto con SIMD (CE-4302)

Este taller implementa un sistema de conversión de texto (mayúsculas ↔ minúsculas) utilizando procesamiento vectorial SIMD (Single Instruction, Multiple Data) sobre la arquitectura x86 con AVX2.  
El objetivo es comparar el rendimiento entre una versión serial y una versión SIMD, evaluando el impacto de la alineación de datos, el tamaño de la cadena y el porcentaje de caracteres alfabéticos.  
Además, se realiza una traducción del código SIMD a otra ISA (ARM NEON) y su validación mediante Compiler Explorer, para demostrar la portabilidad de los algoritmos vectoriales.

---

## Requisitos

- Uinux 
- GCC / G++ con soporte AVX2  
- Python 3.10 o superior  
- Librerías de Python:
  ```bash
  pip install numpy matplotlib


## Compilacion unitaria
# Generador de cadenas
g++ -O3 -o string_generator string_generator.cpp

# Conversión serial
g++ -O3 -o case_converter_serial case_converter_serial.cpp

# Conversión SIMD (AVX2)
g++ -O3 -mavx2 -march=native -o case_converter_SIMD case_converter_SIMD.cpp


## Ejecucion manual
./string_generator --size 1048576 --align 16 --alpha 20 --out test.bin
./case_converter_serial --mode upper -i test.bin
./case_converter_SIMD --mode upper -i test.bin


## Ejecucion automatizada y generacion de graficas

El siguiente script ejecuta todas las pruebas de forma automática, variando el tamaño, la alineación y el porcentaje de caracteres alfabéticos.
También genera las gráficas comparativas de rendimiento.

python3 benchmark_case_converter.py

## RESULTADOS
figures/
 ├── figure_alpha_0.png
 ├── figure_alpha_10.png
 ├── figure_alpha_20.png
 ...
 └── figure3_alpha_vs_perf.png

