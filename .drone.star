#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

_triggers = { "branch": [ "master", "develop" ] }


def pipeline(
    name,
    toolset,
    build_type,
    cxxstd,
    generator,
    build_shared_libs,
    cxxflags = ""
):

    common_args = "--build-type {} --cxxstd {} --toolset {} --generator '{}' --build-shared-libs {} --cxxflags={}".format(
        build_type, cxxstd, toolset, generator, build_shared_libs, cxxflags)

    return {
        "name": name,
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
                "python tools\\ci.py setup-boost --source-dir=%CD%",
                "python tools\\ci.py build-b2-distro --toolset {}".format(toolset),
                "python tools\\ci.py build-cmake-distro {} --integration-tests 0".format(common_args),
                "python tools\\ci.py run-cmake-add-subdirectory-tests {}".format(common_args),
                "python tools\\ci.py run-cmake-find-package-tests {}".format(common_args),
                "python tools\\ci.py run-cmake-b2-find-package-tests {}".format(common_args),
            ]
        }]
    }


def main(ctx):
    return [
        pipeline(
            name = "MSVC 14.2",
            toolset = "msvc-14.2",
            build_type = "Release",
            cxxstd = "20",
            generator = "Visual Studio 16 2019",
            build_shared_libs = 0,
        ),
    ]
