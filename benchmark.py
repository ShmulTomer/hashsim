import subprocess
import time
import matplotlib.pyplot as plt

TUPLE_SIZES = [5000]
COLLISION_MODES = {
    'no_collision': '--no_collision',
    'some_collision': '--some_collision',
    'max_collision': '--max_collision'
}
results = {
    'standard' : { 'no_collision': [],
    'some_collision': [],
    'max_collision': []
    },
    'elastic_hashing' : { 
        'no_collision': [],
        'some_collision': [],
        'max_collision': []
    } 
}

map_types = {
    'standard' : './46-buzzdb.out', 
    'elastic_hashing' : './45-buzzdb.out'
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

def run_db(collision_flag, map_type): 
    subprocess.run(["rm", "-f", "buzzdb.dat"])
    result = subprocess.run([map_types[map_type], collision_flag], capture_output=True, text=True)
    for line in result.stdout.splitlines():
        if "Elapsed time:" in line:
            return int(line.split("Elapsed time:")[1].split()[0])
    return None

# Run benchmarks
for map in map_types:
    for mode, flag in COLLISION_MODES.items():
        for size in TUPLE_SIZES:
            print(f"Running: mode={mode}, size={size}, with map={map}")
            generate_output_txt(size, mode)
            elapsed = run_db(flag, map)
            print(f"  -> {elapsed} µs")
            results[map][mode].append(elapsed)

# Plotting with markers
markers = {
    'no_collision': 'o',
    'some_collision': 's',
    'max_collision': '^'
}
for map in results:
    for mode in results[map]:
        plt.plot(
            TUPLE_SIZES,
            results[map][mode],
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
    plt.savefig("performance_plot_{map}.png")
    plt.show()
