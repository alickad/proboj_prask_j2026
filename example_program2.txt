nech kamene = 0
nech papiere = 0
nech noznice = 0

ak minule kamen
	kamene = kamene + 1
koniec
ak minule papier
	papiere = papiere + 1
koniec
ak minule noznice
	noznice = noznice + 1
koniec

ak kamene >= papiere aj kamene >= noznice
	zahraj papier
koniec
ak papiere >= kamene aj papiere >= noznice
	zahraj noznice
koniec
ak noznice >= kamene aj noznice >= papiere
	zahraj kamen
koniec
