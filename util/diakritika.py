from unicodedata import normalize

def add_mapping(source, dest):
	utf8 = source.encode("utf8")
	assert len(utf8) == 2
	mappings[(utf8[0] << 8) + utf8[1]] = dest

slovencina = "áäčďéíĺľňóôŕšťúýž"
slovencina += slovencina.upper()
mappings = {}

for ch in slovencina:
	base, accent = normalize("NFD", ch)

	add_mapping(ch, base)
	add_mapping(accent, "\\0")

for key, val in sorted(mappings.items()):
	print(f"\t{{{key}, '{val}'}},")

print(len(mappings.items()))