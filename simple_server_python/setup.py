from setuptools import find_packages, setup
from package import Package

setup(name='simple_server',
  version='0.1',
  author='Sean Choi',
  author_email='yo2seol@stanford.edu',
  license='MIT',
  packages=find_packages(),
  include_package_data=True,
  cmdclass={
    "package": Package
  },
  zip_safe=False)
