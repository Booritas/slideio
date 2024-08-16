import os
import re
import sys
import platform
import subprocess
import fnmatch
import shutil

from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext
import setuptools.command.build_py
from distutils.version import LooseVersion
from ctypes.util import find_library

version = '2.6.'
vrs_sub = '1'

if os.environ.get('CI_PIPELINE_IID'):
    ci_id = os.environ['CI_PIPELINE_IID']
    if (isinstance(ci_id, str) and len(ci_id)>0) or isinstance(ci_id, int):
        vrs_sub = ci_id

version = version + vrs_sub

source_dir= os.path.abspath('../../')
build_dir= os.path.abspath('../../build_py')

def get_platform():
    platforms = {
        'linux' : 'Linux',
        'linux1' : 'Linux',
        'linux2' : 'Linux',
        'darwin' : 'Macos',
        'win32' : 'Windows'
    }
    if sys.platform not in platforms:
        return sys.platform
    return platforms[sys.platform]

PLATFORM = get_platform()

REDISTR_LIBS = []

if PLATFORM=='Windows':
    REDISTR_LIBS = [
        'concrt140.dll',
        'msvcp140.dll',
        'msvcp140_1.dll',
        'msvcp140_2.dll',
        'msvcp140_codecvt_ids.dll',
        'vccorlib140.dll',
        'vcruntime140.dll',
        'vcruntime140_1.dll']


def find_shared_libs(dir, pattern):
    matches = []
    for root, dirnames, filenames in os.walk(dir):
        for filename in fnmatch.filter(filenames, pattern):
            matches.append(os.path.join(root, filename))
    return matches

# Get the long description from the README file
here = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(here, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

# Get requirements from requirements-dev.txt file
with open(os.path.join(here, 'requirements-dev.txt')) as f:
    requirements_dev = f.read().replace('==', '>=').splitlines()


class CMakeExtension(Extension):
    def __init__(self, name, source_dir='', build_dir=None):
        Extension.__init__(self, name, sources=[])
        self.source_dir = os.path.abspath(source_dir)
        self.build_dir = os.path.abspath(build_dir)

class CMakeBuild(build_ext):
    def run(self):
        try:
            out = subprocess.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: " +
                ", ".join(e.name for e in self.extensions)
            )

        if platform.system() == "Windows":
            cmake_version = LooseVersion(
                re.search(r'version\s*([\d.]+)', out.decode()).group(1)
            )
            if cmake_version < '3.1.0':
                raise RuntimeError("CMake >= 3.1.0 is required on Windows")

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        if not (ext.build_dir is None):
            self.build_temp = ext.build_dir
        extdir = os.path.abspath(os.path.dirname(
            self.get_ext_fullpath(ext.name)))
        cmake_args = [
          '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir,
            '-DPYTHON_EXECUTABLE=' + sys.executable
        ]

        cfg = 'Release'
        build_args = ['--config', cfg, "--target", "slideiopybind"]

        if platform.system() == "Windows":
            cmake_args += [
                '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}'.format(
                    cfg.upper(), extdir
                )
            ]
            if sys.maxsize > 2**32:
                cmake_args += ['-A', 'x64']
            cmake_args += ['-G', 'Visual Studio 16 2019']
            build_args += ['--', '/m']
        else:
            cmake_args += ['-DCMAKE_BUILD_TYPE=' + cfg]
            build_args += ['--', '-j2']

        env = os.environ.copy()
        env['CXXFLAGS'] = '{} -DVERSION_INFO=\\"{}\\"'.format(
            env.get('CXXFLAGS', ''),
            self.distribution.get_version()
        )
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        subprocess.check_call(
            ['cmake', ext.source_dir] + cmake_args,
            cwd=self.build_temp, env=env
        )
        subprocess.check_call(
            ['cmake', '--build', '.'] + build_args,
            cwd=self.build_temp
        )
        patterns = ["*.so","*.so.*"]
        if PLATFORM == "Windows":
            patterns = ["*.dll","*.pyd"]
        elif PLATFORM == "Macos":
            patterns = ["*.so", "*.dylib"]
            
        print("----Look for shared libraries int directory", self.build_temp)
        extra_files = []
        for pattern in patterns:
            files = find_shared_libs(self.build_temp, pattern)
            if len(files)>0:
                extra_files.extend(files)

        print("----Found libraries:", extra_files)
        if not os.path.exists(extdir):
            os.makedirs(extdir)
        for fl in extra_files:
            file_name = os.path.basename(fl)
            destination = os.path.join(extdir, file_name)
            print("Copy",fl,"->",destination)
            shutil.copyfile(fl, destination)

        for lib in REDISTR_LIBS:
            shutil.copy(find_library(lib), extdir)
        

setup(
    name='slideio',
    version=version,
    author='Stanislav Melnikov',
    author_email='stanislav.melnikov@gmail.com',
    description='Reading of medical images',
    long_description=long_description,
    ext_modules=[CMakeExtension(name = 'slideio', source_dir=source_dir, build_dir=build_dir)],
    cmdclass=dict(build_ext=CMakeBuild),
    packages=find_packages(),
    project_urls={
        'Documentation':'http://slideio.com',
        "Source Code": "https://github.com/Booritas/slideio"
    },
    keywords = 'images, pathology, tissue, medical, czi, svs, afi, scn, ndpi',
    package_data={},
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: BSD License',
        'Topic :: Scientific/Engineering',
        'Topic :: Scientific/Engineering :: Bio-Informatics',
        'Topic :: Scientific/Engineering :: Image Recognition',
        'Topic :: Scientific/Engineering :: Medical Science Apps.',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
    ],
    install_requires=['numpy'],
    extras_require={},
    data_files=[(
        '.', [
            'requirements-dev.txt'
        ]
    )],
    zip_safe=False,
)