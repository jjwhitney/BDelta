from bdelta import BDelta

a = u"The quick brown fox jumped over the lazy dog"
b = u"The quick drowned fox jumped over the lazy dog"

b = BDelta(a, b)

b.b_pass(13, 27, 0) # Find all matches that are at least 27 chars long
print list(b.matches()) # [(15, 17, 29)])

b.b_pass(3, 5, 0) # Find all matches that are at least 5 chars long
print list(b.matches()) # [(0, 0, 10), (15, 17, 29)]

b.b_pass(2, 3, 0) # Find all matches that are at least 3 chars long
print list(b.matches()) # [(0, 0, 10), (11, 11, 4), (15, 17, 29)]
