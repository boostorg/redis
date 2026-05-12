#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

_triggers = { "branch": [ "master", "develop" ] }


def pipeline():

    return {
        "name": "Windows test",
        "kind": "pipeline",
        "type": "docker",
        "trigger": _triggers,
        "platform": {
            "os": "windows"
        },
        "clone": {
            "retries": 5
        },
        "node": {},
        "steps": [{
            "name": "Build and run",
            "image": "cppalliance/dronevs2019:2",
            "pull": "if-not-exists",
            "commands": [
                "choco install -y openssl",
    #             python tools\ci.py setup-boost --source-dir=%CD%
    #             python tools\ci.py build-b2-distro --toolset %TOOLSET%

    #   python tools\ci.py build-cmake-distro
    #   --build-type %BUILD_TYPE%
    #   --cxxstd %CXXSTD%
    #   --toolset %TOOLSET%
    #   --generator "%GENERATOR%"
    #   --build-shared-libs %BUILD_SHARED_LIBS%
    #   --integration-tests 0
    #   --cxxflags=%CXXFLAGS%

    #   python tools\ci.py run-cmake-add-subdirectory-tests
    #   --build-type %BUILD_TYPE%
    #   --cxxstd %CXXSTD%
    #   --toolset %TOOLSET%
    #   --generator "%GENERATOR%"
    #   --build-shared-libs %BUILD_SHARED_LIBS%
    #   --cxxflags=%CXXFLAGS%
  

    #   python tools\ci.py run-cmake-find-package-tests
    #   --build-type %BUILD_TYPE%
    #   --cxxstd %CXXSTD%
    #   --toolset %TOOLSET%
    #   --generator "%GENERATOR%"
    #   --build-shared-libs %BUILD_SHARED_LIBS%
    #   --cxxflags=%CXXFLAGS%
  

    #   python tools\ci.py run-cmake-b2-find-package-tests
    #   --build-type %BUILD_TYPE%
    #   --cxxstd %CXXSTD%
    #   --toolset %TOOLSET%
    #   --generator "%GENERATOR%"
    #   --build-shared-libs %BUILD_SHARED_LIBS%
    #   --cxxflags=%CXXFLAGS%
            ]
        }]
    }


def main(ctx):
    return [
        pipeline()
    ]

