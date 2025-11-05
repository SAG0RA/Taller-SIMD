// case_converter_SIMD.cpp
// Compilar:
//   g++ -O3 -mavx2 -std=c++17 -o case_converter_SIMD case_converter_SIMD.cpp
//
// Uso:
//   ./case_converter_SIMD --mode upper -i cadena.bin
//   ./case_converter_SIMD --mode lower -n 1000000 --alpha 80
//
// Procesa texto con SIMD AVX2 (256 bits) y mide tiempo, memoria real (VmRSS) y checksum.

#include <immintrin.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <random>
#include <cstdint>
#include <string>

using namespace std;

// =============================================================
// Utilidades
// =============================================================

// --- Checksum FNV-1a (para validar la salida ente serial y SIMD)
uint64_t fnv1a64(const uint8_t* data, size_t len) {
    const uint64_t OFFSET = 1469598103934665603ULL;
    const uint64_t PRIME  = 1099511628211ULL;
    uint64_t h = OFFSET;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= PRIME;
    }
    return h;
}

// --- Lectura de uso de memoria desde /proc/self/status
double get_memory_usage_kb_proc() {
    std::ifstream stat("/proc/self/status");
    std::string line;
    while (std::getline(stat, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            double kb;
            sscanf(line.c_str(), "VmRSS: %lf", &kb);
            return kb;
        }
    }
    return 0.0;
}

// --- Cargar archivo binario
bool load_file(const string& filename, vector<uint8_t>& buffer) {
    ifstream ifs(filename, ios::binary);
    if (!ifs) return false;
    ifs.seekg(0, ios::end);
    streamoff sz = ifs.tellg();
    ifs.seekg(0, ios::beg);
    if (sz < 0) return false;
    buffer.resize((size_t)sz);
    ifs.read(reinterpret_cast<char*>(buffer.data()), (streamsize)sz);
    return true;
}

// --- Generar buffer aleatorio
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

// =============================================================
// Conversion serial (solo para el tail < 32 bytes)
// =============================================================
static inline void case_convert_serial_tail(uint8_t* ptr, size_t len, bool to_upper) {
    if (to_upper) {
        for (size_t i = 0; i < len; ++i) {
            uint8_t c = ptr[i];
            if (c >= 'a' && c <= 'z') ptr[i] = c - 0x20;
        }
    } else {
        for (size_t i = 0; i < len; ++i) {
            uint8_t c = ptr[i];
            if (c >= 'A' && c <= 'Z') ptr[i] = c + 0x20;
        }
    }
}

// =============================================================
// Conversion SIMD AVX2
// =============================================================
void case_convert_simd_avx2(uint8_t* text, size_t len, bool to_upper) {
    const size_t V_BYTES = 32; // 256 bits
    size_t nvec = len / V_BYTES;
    size_t offset = 0;

    // Constantes
    const __m256i v_0x80   = _mm256_set1_epi8((char)0x80);
    const __m256i v_delta  = _mm256_set1_epi8((char)0x20);
    const __m256i v_a      = _mm256_set1_epi8((char)'a');
    const __m256i v_z      = _mm256_set1_epi8((char)'z');
    const __m256i v_A      = _mm256_set1_epi8((char)'A');
    const __m256i v_Z      = _mm256_set1_epi8((char)'Z');

    for (size_t i = 0; i < nvec; ++i) {
        uint8_t* ptr = text + offset;

        // Cargar 32 bytes (no alineado)
        __m256i v = _mm256_loadu_si256((const __m256i*)ptr);

        // Convertir a dominio signed mediante XOR 0x80
        __m256i v_off = _mm256_xor_si256(v, v_0x80);

        // Rango para 'a'..'z'
        __m256i v_a_minus1 = _mm256_sub_epi8(v_a, _mm256_set1_epi8(1));
        __m256i v_z_plus1 = _mm256_add_epi8(v_z, _mm256_set1_epi8(1));
        __m256i mask_lo = _mm256_cmpgt_epi8(v_off, v_a_minus1);
        __m256i mask_hi = _mm256_cmpgt_epi8(v_z_plus1, v_off);
        __m256i is_lower = _mm256_and_si256(mask_lo, mask_hi);

        // Rango para 'A'..'Z'
        __m256i v_A_minus1 = _mm256_sub_epi8(v_A, _mm256_set1_epi8(1));
        __m256i v_Z_plus1 = _mm256_add_epi8(v_Z, _mm256_set1_epi8(1));
        __m256i mask_A_lo = _mm256_cmpgt_epi8(v_off, v_A_minus1);
        __m256i mask_A_hi = _mm256_cmpgt_epi8(v_Z_plus1, v_off);
        __m256i is_upper = _mm256_and_si256(mask_A_lo, mask_A_hi);

        if (to_upper) {
            __m256i delta_mask = _mm256_and_si256(is_lower, v_delta);
            __m256i result = _mm256_sub_epi8(v, delta_mask);
            _mm256_storeu_si256((__m256i*)ptr, result);
        } else {
            __m256i delta_mask = _mm256_and_si256(is_upper, v_delta);
            __m256i result = _mm256_add_epi8(v, delta_mask);
            _mm256_storeu_si256((__m256i*)ptr, result);
        }

        offset += V_BYTES;
    }

    // Procesar tail restante (<32 bytes)
    size_t tail = len - offset;
    if (tail) case_convert_serial_tail(text + offset, tail, to_upper);
}

// =============================================================
// main
// =============================================================
int main(int argc, char** argv) {
    string input_file = "";
    string mode = "upper";
    size_t gen_size = 0;
    double alpha = 80.0;

    // Parseo simple de argumentos
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--mode" && i+1 < argc) mode = argv[++i];
        else if (a == "-i" && i+1 < argc) input_file = argv[++i];
        else if (a == "-n" && i+1 < argc) gen_size = stoull(argv[++i]);
        else if (a == "--alpha" && i+1 < argc) alpha = stod(argv[++i]);
        else {
            cerr << "Uso: " << argv[0] << " --mode {upper|lower} (-i file | -n size) [--alpha pct]\n";
            return 1;
        }
    }

    bool to_upper = (mode == "upper");
    vector<uint8_t> data;

    if (!input_file.empty()) {
        if (!load_file(input_file, data)) {
            cerr << "Error leyendo archivo: " << input_file << "\n";
            return 1;
        }
        cout << "Archivo cargado: " << input_file << " (" << data.size() << " bytes)\n";
    } else if (gen_size > 0) {
        data = generate_random_buffer(gen_size, alpha);
        cout << "Buffer generado: " << gen_size << " bytes, " << alpha << "% letras\n";
    } else {
        cerr << "Debe proporcionar -i <archivo> o -n <tamaño>\n";
        return 1;
    }

    // --- Medicion de tiempo y memoria ---
    double mem_before = get_memory_usage_kb_proc();
    auto t0 = chrono::high_resolution_clock::now();

    case_convert_simd_avx2(data.data(), data.size(), to_upper);

    auto t1 = chrono::high_resolution_clock::now();
    double mem_after = get_memory_usage_kb_proc();
    chrono::duration<double, milli> elapsed = t1 - t0;
    uint64_t checksum = fnv1a64(data.data(), data.size());

    // --- Resultados ---
    cout << fixed << setprecision(3);
    cout << "\n=== Resultados SIMD (AVX2) ===\n";
    cout << "Modo: " << (to_upper ? "TO_UPPER" : "TO_LOWER") << "\n";
    cout << "Tamaño: " << data.size() << " bytes\n";
    cout << "Tiempo: " << elapsed.count() << " ms\n";
    cout << "Memoria usada (VmRSS): " << (mem_after - mem_before) << " KB\n";
    cout << "Checksum: 0x" << hex << checksum << dec << "\n";

    return 0;
}
