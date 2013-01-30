from distutils.core import setup, Extension
from Cython.Distutils import build_ext
from version import get_git_version
import sys

bdelta_module = Extension(
	"bdelta",
	["src/bdelta.pyx", "src/libbdelta.cpp"],
	define_macros = [('TOKEN_SIZE', '2')],
	extra_compile_args = ['/EHsc'] if sys.platform == 'win32' else None
)

setup(
    name = 'BDelta',
    version = get_git_version(),
    description = 'Python Bindings for BDelta',
    author = 'John Whitney',
    author_email = 'jjw@deltup.org',
    url = 'http://bdelta.org',
    cmdclass = {'build_ext': build_ext},
    ext_modules = [bdelta_module]
)
