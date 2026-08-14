nech X = 1
nech Y = 2
nech Z = X + Y
nech akcia = 0
nech tah = 0

program:
tah = tah + 1
X = Y/2*3
Z = X + Z

akcia = (Z + tah) % 3

ak akcia == 1
zahraj KAMEN
koniec
ak akcia == 2
zahraj PAPIER
koniec
zahraj NOZNICE
