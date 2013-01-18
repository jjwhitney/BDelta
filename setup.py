from distutils.core import setup, Extension
from Cython.Distutils import build_ext
from version import get_git_version

ext_modules = [Extension(
	"bdelta",
	["src/bdelta.pyx", "src/libbdelta.cpp"],
	define_macros=[('TOKEN_SIZE', '2')],
)]

setup(
    name = 'BDelta',
    version = get_git_version(),
    description = 'Python Bindings for BDelta',
    author = 'John Whitney',
    author_email = 'jjw@deltup.org',
    url = 'http://bdelta.org',
    cmdclass = {'build_ext': build_ext},
    ext_modules = ext_modules
)
