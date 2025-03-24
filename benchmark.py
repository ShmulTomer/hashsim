import subprocess
import time
import matplotlib.pyplot as plt

TUPLE_SIZES = [5000, 10000, 30000, 50000]
COLLISION_MODES = {
    'no_collision': '--no_collision',
    'some_collision': '--some_collision',
    'max_collision': '--max_collision'
}
results = {
    'no_collision': [],
    'some_collision': [],
    'max_collision': []
}

def generate_output_txt(n, mode):
    with open("output.txt", "w") as f:
        for i in range(1, n+1):
            # if mode == 'no_collision':
            #     key = i
            # elif mode == 'some_collision':
            #     key = i % (n // 10 or 1)  # 10% unique keys
            # elif mode == 'max_collision':
            #     key = 1  # all keys the same
            key = i
            f.write(f"{key} {i}\n")

def run_db(collision_flag):
    subprocess.run(["rm", "-f", "buzzdb.dat"])
    result = subprocess.run(["./45-buzzdb.out", collision_flag], capture_output=True, text=True)
    for line in result.stdout.splitlines():
        if "Elapsed time:" in line:
            return int(line.split("Elapsed time:")[1].split()[0])
    return None

# Run benchmarks
for mode, flag in COLLISION_MODES.items():
    for size in TUPLE_SIZES:
        print(f"Running: mode={mode}, size={size}")
        generate_output_txt(size, mode)
        elapsed = run_db(flag)
        print(f"  -> {elapsed} µs")
        results[mode].append(elapsed)

# Plotting with markers
markers = {
    'no_collision': 'o',
    'some_collision': 's',
    'max_collision': '^'
}

for mode in results:
    plt.plot(
        TUPLE_SIZES,
        results[mode],
        label=mode.replace('_', ' ').title(),
        marker=markers[mode],
        markersize=6,
        linewidth=2
    )

plt.xlabel("Number of Tuples")
plt.ylabel("Elapsed Time (microseconds)")
plt.title("Hash Table Operation Time vs. Tuple Count and Collision Mode")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("performance_plot.png")
plt.show()
