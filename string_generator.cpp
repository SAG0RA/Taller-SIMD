// string_generator.cpp
// Compilar: g++ -O2 -std=c++17 -o string_generator string_generator.cpp
//
// Ejemplo:
//   ./string_generator --size 1000 --align 32 --alpha 80 --out texto.bin
//
// Genera una cadena UTF-8 aleatoria con alineamiento y porcentaje de letras definido.

#include <iostream>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdlib>
#include <string>

using namespace std;

int main(int argc, char** argv) {
    size_t size = 0;
    size_t alignment = 1;
    double percent_alpha = 0.0;
    string outfile = "";

    // === Leer argumentos ===
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--size" && i + 1 < argc) size = stoull(argv[++i]);
        else if (arg == "--align" && i + 1 < argc) alignment = stoull(argv[++i]);
        else if (arg == "--alpha" && i + 1 < argc) percent_alpha = stod(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) outfile = argv[++i];
        else {
            cerr << "Uso: ./string_generator --size <n> --align <n> --alpha <0-100> [--out archivo]\n";
            return 1;
        }
    }

    if (size == 0) {
        cerr << "Debe especificar --size\n";
        return 1;
    }
    if (percent_alpha < 0 || percent_alpha > 100) {
        cerr << "--alpha debe estar entre 0 y 100\n";
        return 1;
    }

    // === Reservar memoria alineada ===
    char* buffer = nullptr;
    int res = posix_memalign((void**)&buffer, alignment, size);
    if (res != 0 || buffer == nullptr) {
        cerr << "Error al asignar memoria alineada\n";
        return 1;
    }

    // === Generador aleatorio ===
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<double> prob(0.0, 1.0);
    uniform_int_distribution<int> upper('A', 'Z');
    uniform_int_distribution<int> lower('a', 'z');
    uniform_int_distribution<int> printable(32, 126);

    // === Generar cadena ===
    for (size_t i = 0; i < size; ++i) {
        if (prob(gen) < percent_alpha / 100.0) {
            // 50% mayúscula, 50% minúscula
            if (prob(gen) < 0.5)
                buffer[i] = (char)upper(gen);
            else
                buffer[i] = (char)lower(gen);
        } else {
            buffer[i] = (char)printable(gen);
        }
    }

    // === Guardar o mostrar resultado ===
    if (!outfile.empty()) {
        ofstream ofs(outfile, ios::binary);
        ofs.write(buffer, size);
        ofs.close();
        cout << "Archivo generado: " << outfile << " (" << size << " bytes, "
             << percent_alpha << "% letras, alineado a " << alignment << " bytes)\n";
    } else {
        // Muestra primeros caracteres
        cout << "Ejemplo de salida (" << size << " bytes):\n";
        for (size_t i = 0; i < min(size_t(100), size); ++i)
            cout << buffer[i];
        cout << "\n";
    }

    free(buffer);
    return 0;
}
