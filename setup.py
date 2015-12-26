from distutils.core import setup, Extension

module1 = Extension('lazybrush',
                    sources = ['lazybrush.c'])

setup (name = 'LazyBrush',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [module1])
