// case_converter_serial.cpp
// Compilar: g++ -O3 -std=c++17 -o case_converter_serial case_converter_serial.cpp
//
// Uso:
//   ./case_converter_serial --mode upper  -i texto.bin
//   ./case_converter_serial --mode lower  -i texto.bin
//   ./case_converter_serial --mode upper  -n 1000000
//
// Mide tiempo de ejecución, memoria utilizada y genera un checksum para validación.

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sys/resource.h>
#include <random>
#include <unistd.h> 


using namespace std;

// ---------------------------------------------------------
// Función serial de conversión
// ---------------------------------------------------------
void case_convert_serial(uint8_t* text, size_t len, bool to_upper) {
    if (to_upper) {
        for (size_t i = 0; i < len; ++i) {
            if (text[i] >= 'a' && text[i] <= 'z')
                text[i] -= 0x20;
        }
    } else {
        for (size_t i = 0; i < len; ++i) {
            if (text[i] >= 'A' && text[i] <= 'Z')
                text[i] += 0x20;
        }
    }
}

// ---------------------------------------------------------
// Checksum simple FNV-1a (para validar igualdad entre serial y SIMD)
// ---------------------------------------------------------
uint64_t fnv1a64(const uint8_t* data, size_t len) {
    const uint64_t FNV_OFFSET = 1469598103934665603ULL;
    const uint64_t FNV_PRIME  = 1099511628211ULL;
    uint64_t hash = FNV_OFFSET;
    for (size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

// ---------------------------------------------------------
// Cargar archivo completo a memoria
// ---------------------------------------------------------
bool load_file(const string& filename, vector<uint8_t>& buffer) {
    ifstream file(filename, ios::binary);
    if (!file) return false;
    file.seekg(0, ios::end);
    size_t size = file.tellg();
    file.seekg(0, ios::beg);
    buffer.resize(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return true;
}

// ---------------------------------------------------------
// Generar buffer aleatorio para pruebas
// ---------------------------------------------------------
vector<uint8_t> generate_random_buffer(size_t size, double alpha_ratio) {
    vector<uint8_t> buf(size);
    mt19937 rng(random_device{}());
    uniform_real_distribution<double> p(0.0, 1.0);
    uniform_int_distribution<int> upper('A', 'Z');
    uniform_int_distribution<int> lower('a', 'z');
    uniform_int_distribution<int> printable(32, 126);

    for (size_t i = 0; i < size; ++i) {
        if (p(rng) < alpha_ratio / 100.0) {
            buf[i] = (p(rng) < 0.5) ? upper(rng) : lower(rng);
        } else {
            buf[i] = printable(rng);
        }
    }
    return buf;
}

// ---------------------------------------------------------
// Medición de uso de memoria residente (en KB)
// ---------------------------------------------------------
double get_memory_usage_kb() {
    long rss = 0L;
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp == nullptr)
        return 0.0;
    if (fscanf(fp, "%*s%ld", &rss) != 1) {
        fclose(fp);
        return 0.0;
    }
    fclose(fp);
    long page_size_kb = sysconf(_SC_PAGESIZE) / 1024; // tamaño de página
    return rss * page_size_kb;
}

// ---------------------------------------------------------
// Programa principal
// ---------------------------------------------------------
int main(int argc, char** argv) {
    string input_file = "";
    string mode = "upper";
    size_t gen_size = 0;
    double alpha = 80.0;

    // Parsear argumentos
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
        else if (arg == "-i" && i + 1 < argc) input_file = argv[++i];
        else if (arg == "-n" && i + 1 < argc) gen_size = stoull(argv[++i]);
        else if (arg == "--alpha" && i + 1 < argc) alpha = stod(argv[++i]);
    }

    bool to_upper = (mode == "upper");

    // Cargar datos
    vector<uint8_t> data;
    if (!input_file.empty()) {
        if (!load_file(input_file, data)) {
            cerr << "Error: no se pudo leer el archivo " << input_file << endl;
            return 1;
        }
        cout << "Archivo cargado: " << input_file << " (" << data.size() << " bytes)\n";
    } else if (gen_size > 0) {
        data = generate_random_buffer(gen_size, alpha);
        cout << "Cadena aleatoria generada (" << gen_size << " bytes, "
             << alpha << "% letras)\n";
    } else {
        cerr << "Debe usar -i <archivo> o -n <tamaño>\n";
        return 1;
    }

    // Medir memoria antes
    double mem_before = get_memory_usage_kb();

    // --- Medir tiempo ---
    auto t0 = chrono::high_resolution_clock::now();
    case_convert_serial(data.data(), data.size(), to_upper);
    auto t1 = chrono::high_resolution_clock::now();

    // --- Resultados ---
    chrono::duration<double, milli> elapsed = t1 - t0;
    double mem_after = get_memory_usage_kb();
    uint64_t checksum = fnv1a64(data.data(), data.size());

    cout << fixed << setprecision(3);
    cout << "\n=== Resultados ===\n";
    cout << "Modo: " << (to_upper ? "TO_UPPER" : "TO_LOWER") << endl;
    cout << "Tamaño del texto: " << data.size() << " bytes\n";
    cout << "Tiempo de ejecución: " << elapsed.count() << " ms\n";
    cout << "Uso de memoria: " << (mem_after - mem_before) << " KB (aprox.)\n";
    cout << "Checksum: 0x" << hex << checksum << dec << "\n";

    return 0;
}
