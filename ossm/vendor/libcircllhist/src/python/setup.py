from setuptools import setup, find_packages

with open('README.md') as file:
    long_description = file.read()

setup(
    name="circllhist",
    long_description=long_description,
    long_description_content_type='text/markdown',
    version="0.3.2",
    description="OpenHistogram log-linear histogram library",
    maintainer="Circonus Packaging",
    maintainer_email="packaging@circonus.com",
    url="https://github.com/openhistogram/libcircllhist",
    install_requires=['cffi'],
    packages=['circllhist'],
    classifiers=[
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: POSIX"
    ],
    python_requires=">=2.7",
)
