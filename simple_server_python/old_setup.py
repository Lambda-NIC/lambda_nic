from setuptools import setup

setup(name='simple_server',
  version='0.1',
  author='Sean Choi',
  author_email='yo2seol@stanford.edu',
  license='MIT',
  packages=['simple_server'],
  install_requires=[
    'netifaces',
    'Pillow',
    'numpy'
  ],
  dependency_links=['https://github.com/idanmo/python-memcached-udp/archive/master.zip'],
  zip_safe=False)
