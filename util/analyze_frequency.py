import sys

# the first argv is the filename, it doesn't count
if len(sys.argv) != 2:
    sys.exit(f"This scripts takes exactly one argument, got {len(sys.argv) - 1}")

with open(sys.argv[1], 'r') as f:
    content = f.readlines()
name1, name2 = content[0][:-1], content[1][:-1]
turns1 = {'KAMEN': 0, 'PAPIER': 0, 'NOZNICE': 0, 'ERROR': 0}
turns2 = {'KAMEN': 0, 'PAPIER': 0, 'NOZNICE': 0, 'ERROR': 0}

for line in content[3:]:
    t1, t2 = line.split()
    if t1 in turns1:
        turns1[t1] += 1
    else:
        print(f"Unknown turn: {t1}")

    if t2 in turns2:
        turns2[t2] += 1
    else:
        print(f"Unknown turn: {t2}")
print(f"{name1}: {turns1}")
print(f"{name2}: {turns2}")

