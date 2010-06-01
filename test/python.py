import sys
sys.path.append("../src")

from bdelta_python import bdelta_SimpleString

a = "abcdefghijklmnopqrstuvwxyz"
b = "abcdefghi1jklmnopqrstuvwxyz"

print bdelta_SimpleString(unicode(a), unicode(b), 8)

print bdelta_SimpleString(u"abcdefghijklmnopqrstuvwxyz", u"AbcdefghijklmnqrstuvwxyZ", 4)
print bdelta_SimpleString(u"abcdefghijklmnopqrstuvwxyz", u"abcdefghijklmnopqrstuvwxyz", 4)

