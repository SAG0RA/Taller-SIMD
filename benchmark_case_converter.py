#!/usr/bin/env python3
import subprocess
import csv
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import re
import time
import os

# ================== CONFIGURACIÓN ==================
SIZES = np.geomspace(8, 4096, 25).astype(int)
ALIGNS = [16, 32]
ALPHAS = list(range(0, 100, 10))  # 0%,10%,20%,...90%
REPEATS = 3
EXEC_SERIAL = "./case_converter_serial"
EXEC_SIMD = "./case_converter_SIMD"
GENERATOR = "./string_generator"
RESULTS_CSV = "results_benchmark_all.csv"
OUTPUT_DIR = "figures"
os.makedirs(OUTPUT_DIR, exist_ok=True)
# ===================================================


def extract_time(output: str):
    times = []
    for line in output.splitlines():
        if "Tiempo" in line:
            matches = re.findall(r"([0-9]+(?:\.[0-9]+)?)", line)
            times.extend([float(x) for x in matches])
    if times:
        return times[-1]
    return None


def run_cmd(cmd):
    try:
        out = subprocess.check_output(cmd, shell=True, text=True)
        t = extract_time(out)
        return t
    except subprocess.CalledProcessError:
        return None


def run_avg(cmd):
    vals = []
    for _ in range(REPEATS):
        t = run_cmd(cmd)
        if t and t > 0:
            vals.append(t)
        time.sleep(0.05)
    return np.mean(vals) if vals else None


def main():
    Path(RESULTS_CSV).unlink(missing_ok=True)
    with open(RESULTS_CSV, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["size", "align", "alpha", "time_serial", "time_simd"])

        for alpha in ALPHAS:
            print(f"\n=== Ejecutando pruebas para α={alpha}% ===")
            for align in ALIGNS:
                for size in SIZES:
                    subprocess.run(
                        f"{GENERATOR} --size {size} --align {align} --alpha {alpha} --out temp.bin",
                        shell=True, check=True, stdout=subprocess.DEVNULL
                    )

                    t_serial = run_avg(f"{EXEC_SERIAL} --mode upper -i temp.bin")
                    t_simd = run_avg(f"{EXEC_SIMD} --mode upper -i temp.bin")

                    if t_serial and t_simd:
                        writer.writerow([size, align, alpha, t_serial, t_simd])
                        f.flush()
            print(f"✅ α={alpha}% completado")

    print(f"\nResultados guardados en {RESULTS_CSV}")

    # ===================== FIGURA 2.x =====================
    data = np.genfromtxt(RESULTS_CSV, delimiter=",", names=True)
    for alpha in ALPHAS:
        subset_alpha = data[data["alpha"] == alpha]
        if len(subset_alpha) == 0:
            continue

        plt.figure(figsize=(9, 5))
        plt.plot(subset_alpha["size"], np.ones_like(subset_alpha["size"]),
                 label="Serial Implementation", color="skyblue", linewidth=2)

        subset_16 = subset_alpha[subset_alpha["align"] == 16]
        subset_32 = subset_alpha[subset_alpha["align"] == 32]

        if len(subset_16) > 0:
            norm16 = subset_16["time_simd"] / subset_16["time_serial"]
            plt.plot(subset_16["size"], norm16,
                     label="SIMD Implementation (16-byte Aligned)", color="orange",
                     linewidth=2, marker='o')
        if len(subset_32) > 0:
            norm32 = subset_32["time_simd"] / subset_32["time_serial"]
            plt.plot(subset_32["size"], norm32,
                     label="SIMD Implementation (32-byte Unaligned)", color="brown",
                     linewidth=2, marker='s')

        plt.xscale("log")
        plt.yscale("log")
        plt.ylim(0.0005, 1.2)
        plt.xlabel("Input Vector Size (bytes)")
        plt.ylabel("Normalized Execution Time (SIMD / Serial)")
        plt.title(f"Performance Comparison (α={alpha}%)")
        plt.grid(True, which="both", linestyle="--", alpha=0.6)
        plt.legend()
        plt.tight_layout()
        out_path = os.path.join(OUTPUT_DIR, f"figure_alpha_{alpha}.png")
        plt.savefig(out_path, dpi=200)
        plt.close()
        print(f"🖼  Figura generada: {out_path}")

    

if __name__ == "__main__":
    main()
