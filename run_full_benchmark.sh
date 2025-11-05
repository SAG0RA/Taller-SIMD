#!/usr/bin/env bash
set -e
echo "Iniciando script maestro..."
PROJECT_DIR="$(pwd)"
echo "Directorio actual: $PROJECT_DIR"

if [ ! -d "venv" ]; then
  echo "Creando entorno virtual..."
  python3 -m venv venv
fi

echo "Activando entorno..."
source venv/bin/activate

echo "Instalando dependencias..."
pip install --quiet numpy matplotlib

echo "Compilando programas..."
g++ -O3 -std=c++17 -o string_generator string_generator.cpp
g++ -O3 -std=c++17 -o case_converter_serial case_converter_serial.cpp
g++ -O3 -mavx2 -std=c++17 -o case_converter_SIMD case_converter_SIMD.cpp

echo "Ejecutando benchmark..."
python3 benchmark_case_converter.py

echo "Verificando resultados..."
ls -lh results_benchmark.csv benchmark_plot.png

echo "Benchmark finalizado."
deactivate
