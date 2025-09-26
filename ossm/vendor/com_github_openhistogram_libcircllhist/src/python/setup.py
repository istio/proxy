from setuptools import setup, find_packages

with open('README.md') as file:
    long_description = file.read()

setup(
    name="circllhist",
    long_description=long_description,
    version="0.3.0",
    description="Circonus log-linear histogram library",
    maintainer="Heinrich Hartmann",
    maintainer_email="heinrich.hartmann@circonus.com",
    url="https://github.com/circonus-labs/libcircllhist",
    install_requires=['cffi'],
    packages=['circllhist'],
)
