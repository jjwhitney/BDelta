# First argument should be a test suite folder
# Such folders contain subfolders with files 1 (old
# version), and 2 (new version) of data that you
# want to test with the delta algorithm

import sys
import os
import filecmp

basePath = sys.argv[1]

def isValidFile(x):
    return not x.startswith('.') and os.path.isdir(os.path.join(basePath, x))

testFolders = filter(isValidFile, os.listdir(basePath))
print "Running tests:", testFolders
print "BDelta 0.10 file sizes:"
for test in testFolders:
    def path(fName):
        return os.path.join(basePath, test, fName)
    os.system("../src/bdelta %s %s %s" % (path("1"), path("2"), path("patch.bdelta")))
    os.system("../src/bpatch %s %s %s" % (path("1"), path("2.new"), path("patch.bdelta")))
    print ("%20s:" % test),
    if filecmp.cmp(path("2"), path("2.new"), shallow=False):
        print "%10s" % os.stat(path("patch.bdelta")).st_size,
        os.system("bzip2 %s" % path("patch.bdelta"))
        print "%10s" % os.stat(path("patch.bdelta.bz2")).st_size
    else:
        print "ERROR"
    os.remove(path("patch.bdelta.bz2"))
    os.remove(path("2.new"))
