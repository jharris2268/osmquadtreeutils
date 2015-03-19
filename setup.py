import os
from setuptools import setup, Extension
from Cython.Build import cythonize
mapnik_args = os.popen('mapnik-config --cflags').read()

rbsrc = ['mapnikpbf', 'readblock','simplepbf','primitivegroup','readgeometry']


setup(
    name='osmquadtreeutils',
    version='0.0.1',
    author='James Harris',
    packages=['osmquadtreeutils'],
    ext_modules=[
        Extension('osmquadtreeutils.readblock',
            ['src/%s.cpp' % f for f in rbsrc],
            include_dirs=['/usr/include/python2.7', 'src'],
            library_dirs=['/usr/local/lib', '/usr/lib', '/usr/lib/x86_64-linux-gnu'],
            libraries=['boost_python', 'mapnik',],
            extra_compile_args=['-std=c++11','-fpic', '-Wsign-compare', mapnik_args,]
            ),
        
        
        ]+cythonize("osmquadtreeutils/_readpbf.pyx"),
) 
