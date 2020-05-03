#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, shutil
from conans import ConanFile, tools, AutoToolsBuildEnvironment, MSBuild
from conans.errors import ConanException

MSDEV_PLATFORM_SHORT_NAMES = {
    'x86': 'x86',
    'x86_64': 'x64'
}

class RecklessConan(ConanFile):
    name = 'reckless'
    license = 'MIT'
    version = '3.0.1'
    url = 'https://github.com/mattiasflodin/reckless'
    description = """Reckless is an extremely low-latency, high-throughput logging library."""
    settings = 'arch', 'compiler', 'build_type'
    exports_sources = (
        "reckless/src/*",
        "reckless/include/*",
        "Makefile.conan",
        "reckless.sln",
        "common.props",
        "*.vcxproj"
    )

    def configure(self):
        if self._gcc_compatible() and self.settings.compiler.libcxx == "libstdc++":
            raise ConanException("This package is only compatible with libstdc++11. "
                "Please run with -s compiler.libcxx=libstdc++11.")

    def build(self):
        if self.settings.compiler == "Visual Studio":
            env = MSBuild(self)
            env.build('reckless.sln', use_env=False, targets=['reckless:Rebuild'])
        else:
            env = AutoToolsBuildEnvironment(self)
            env.make(args=['-f', 'Makefile.conan'], target="clean")
            # Why doesn't Conan set CXX?
            vars = env.vars
            vars['CXX'] = str(self.settings.compiler)
            env.make(args=['-f', 'Makefile.conan'], vars=vars)

    def package(self):
        if self._gcc_compatible():
            self.copy('*.a', src='reckless/lib', dst='lib', keep_path=False)
        else:
            platform_short_name = MSDEV_PLATFORM_SHORT_NAMES[str(self.settings.arch)]
            configuration_name = str(self.settings.build_type).lower()
            build_directory = os.path.join('build', '%s-%s' % (platform_short_name, configuration_name))
            self.copy('*.lib', src=build_directory, dst='lib', keep_path=False)
            self.copy('*.pdb', src=build_directory, dst='lib', keep_path=False)

        self.copy('*.hpp', src='reckless/include', dst='include', keep_path=True)
        self.copy('*.cpp', src='reckless/src', dst='src', keep_path=True)
        self.copy('LICENSE.txt', src='src')

    def package_info(self):
        self.cpp_info.libs = ["reckless"]

    def _gcc_compatible(self):
        return str(self.settings.compiler) in ['gcc', 'clang', 'apple-clang']
