from distutils.core import setup, Extension

setup(name='bdelta_python',
      version='0.2.2',
      description='Python Bindings for BDelta',
      author='John Whitney',
      author_email='jjw@deltup.org',
      url='http://deltup.org',
      ext_modules=[Extension('bdelta_python', ['bdelta_python.cpp'])],
     )
