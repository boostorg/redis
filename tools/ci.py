#!/usr/bin/python3

from pathlib import Path
from typing import List, Union
import subprocess
import os
import stat
from shutil import rmtree, copytree, ignore_patterns
import argparse


_is_windows = os.name == 'nt'
_boost_root = Path(os.path.expanduser('~')).joinpath('boost-root')
_b2_command = str(_boost_root.joinpath('b2'))


def _run(args: List[str]) -> None:
    print('+ ', args, flush=True)
    subprocess.run(args, check=True)


def _mkdir_and_cd(path: Path) -> None:
    os.makedirs(str(path), exist_ok=True)
    os.chdir(str(path))


def _cmake_bool(value: bool) -> str:
    return 'ON' if value else 'OFF'


def _remove_readonly(func, path, _):
    os.chmod(path, stat.S_IWRITE)
    func(path)


def _build_prefix_path(*paths: Union[str, Path]) -> str:
    return ';'.join(str(p) for p in paths)


def _str2bool(v: Union[bool, str]) -> bool:
    if isinstance(v, bool):
        return v
    elif v == '1':
        return True
    elif v == '0':
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


def _deduce_boost_branch() -> str:
    # Are we in GitHub Actions?
    if os.environ.get('GITHUB_ACTIONS') is not None:
        ci = 'GitHub Actions'
        ref = os.environ.get('GITHUB_BASE_REF', '') or os.environ.get('GITHUB_REF', '')
        res = 'master' if ref == 'master' or ref.endswith('/master') else 'develop'
    elif os.environ.get('DRONE') is not None:
        ref = os.environ.get('DRONE_BRANCH', '')
        ci = 'Drone'
        res = 'master' if ref == 'master' else 'develop'
    else:
        ci = 'Unknown'
        ref = ''
        res = 'develop'
    
    print('+  Found CI {}, ref={}, deduced branch {}'.format(ci, ref, res))

    return res


def _install_boost(
    source_dir: Path
) -> None:
    assert source_dir.is_absolute()
    assert not _boost_root.exists()
    lib_dir = _boost_root.joinpath('libs', 'redis')
    branch = _deduce_boost_branch()

    # Clone Boost
    _run(['git', 'clone', '-b', branch, '--depth', '1', 'https://github.com/boostorg/boost.git', str(_boost_root)])
    os.chdir(str(_boost_root))

    # Put our library inside boost root
    if lib_dir.exists():
        rmtree(str(lib_dir), onerror=_remove_readonly)
    copytree(
        str(source_dir),
        str(lib_dir),
        ignore=ignore_patterns('__build*__', '.git'),
        dirs_exist_ok=True
    )

    # Install Boost dependencies
    _run(["git", "config", "submodule.fetchJobs", "8"])
    _run(["git", "submodule", "update", "-q", "--init", "tools/boostdep"])
    _run(["python", "tools/boostdep/depinst/depinst.py", "--include", "examples", "redis"])

    # Bootstrap
    if _is_windows:
        _run(['cmd', '/q', '/c', 'bootstrap.bat'])
    else:
        _run(['bash', 'bootstrap.sh'])
    _run([_b2_command, 'headers'])


def _build_b2_distro(
    install_prefix: Path
):
    os.chdir(str(_boost_root))
    _run([
        _b2_command,
        '--prefix={}'.format(install_prefix),
        '--with-system',
        '-d0',
        'install'
    ])


def _run_cmake_superproject_tests(
    install_prefix: Path,
    generator: str,
    build_type: str,
    cxxstd: str,
    build_shared_libs: bool = False
):
    _mkdir_and_cd(_boost_root.joinpath('__build_cmake_test__'))
    _run([
        'cmake',
        '-G',
        generator,
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DCMAKE_CXX_STANDARD={}'.format(cxxstd),
        '-DBOOST_INCLUDE_LIBRARIES=redis',
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '-DCMAKE_INSTALL_PREFIX={}'.format(install_prefix),
        '-DBUILD_TESTING=ON',
        '-DBoost_VERBOSE=ON',
        '-DCMAKE_INSTALL_MESSAGE=NEVER',
        '..'
    ])
    _run(['cmake', '--build', '.', '--target', 'tests', '--config', build_type])
    _run(['ctest', '--output-on-failure', '--build-config', build_type])


def _install_cmake_distro(build_type: str):
    _run(['cmake', '--build', '.', '--target', 'install', '--config', build_type])


def _run_cmake_standalone_tests(
    b2_distro: Path,
    generator: str,
    build_type: str,
    cxxstd: str,
    build_shared_libs: bool = False
):
    _mkdir_and_cd(_boost_root.joinpath('libs', 'redis', '__build_standalone__'))
    _run([
        'cmake',
        '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(b2_distro)),
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '-DCMAKE_CXX_STANDARD={}'.format(cxxstd),
        '-G',
        generator,
        '..'
    ])
    _run(['cmake', '--build', '.'])
    _run(['ctest', '--output-on-failure', '--build-config', build_type])


def _run_cmake_add_subdirectory_tests(
    generator: str,
    build_type: str,
    build_shared_libs: bool = False
):
    test_folder = _boost_root.joinpath('libs', 'redis', 'test', 'cmake_test', '__build_cmake_subdir_test__')
    _mkdir_and_cd(test_folder)
    _run([
        'cmake',
        '-G',
        generator,
        '-DBOOST_CI_INSTALL_TEST=OFF',
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '..'
    ])
    _run(['cmake', '--build', '.', '--config', build_type])
    _run(['ctest', '--output-on-failure', '--build-config', build_type])


def _run_cmake_find_package_tests(
    cmake_distro: Path,
    generator: str,
    build_type: str,
    build_shared_libs: bool = False
):
    _mkdir_and_cd(_boost_root.joinpath('libs', 'redis', 'test', 'cmake_test', '__build_cmake_install_test__'))
    _run([
        'cmake',
        '-G',
        generator,
        '-DBOOST_CI_INSTALL_TEST=ON',
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(cmake_distro)),
        '..'
    ])
    _run(['cmake', '--build', '.', '--config', build_type])
    _run(['ctest', '--output-on-failure', '--build-config', build_type])


def _run_cmake_b2_find_package_tests(
    b2_distro: Path,
    generator: str,
    build_type: str,
    build_shared_libs: bool = False
):
    _mkdir_and_cd(_boost_root.joinpath('libs', 'redis', 'test', 'cmake_b2_test', '__build_cmake_b2_test__'))
    _run([
        'cmake',
        '-G',
        generator,
        '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(b2_distro)),
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '-DBUILD_TESTING=ON',
        '..'
    ])
    _run(['cmake', '--build', '.', '--config', build_type])
    _run(['ctest', '--output-on-failure', '--build-config', build_type])


def _run_b2_tests(
    toolset: str,
    cxxstd: str,
    variant: str,
    stdlib: str = 'native',
    address_model: str = '64',
    address_sanitizer: bool = False,
    undefined_sanitizer: bool = False,
):
    os.chdir(str(_boost_root))
    _run([
        _b2_command,
        '--abbreviate-paths',
        'toolset={}'.format(toolset),
        'cxxstd={}'.format(cxxstd),
        'address-model={}'.format(address_model),
        'variant={}'.format(variant),
        'stdlib={}'.format(stdlib),
    ] + (['address-sanitizer=norecover'] if address_sanitizer else [])     # can only be disabled by omitting the arg
      + (['undefined-sanitizer=norecover'] if undefined_sanitizer else []) # can only be disabled by omitting the arg
      + [
        'warnings-as-errors=on',
        '-j4',
        'libs/redis/test',
        'libs/redis/example'
    ])

    # Get Boost
    # Generate "pre-built" b2 distro
    # Build the library, run the tests, and install, from the superproject
    # Library tests, using the b2 Boost distribution generated before (this tests our normal dev workflow)
    # Subdir tests, using add_subdirectory() (lib can be consumed using add_subdirectory)
    # Subdir tests, using find_package with the library installed in the previous step
    # (library can be consumed using find_package on a distro built by cmake)

    # Subdir tests, using find_package with the b2 distribution
    # (library can be consumed using find_package on a distro built by b2)



def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    subp = subparsers.add_parser('install-boost')
    subp.add_argument('--source-dir', type=Path, required=True)
    subp.set_defaults(func=_install_boost)

    subp = subparsers.add_parser('build-b2-distro')
    subp.add_argument('--install-prefix', type=Path, required=True)
    subp.set_defaults(func=_build_b2_distro)

    subp = subparsers.add_parser('run-cmake-superproject-tests')
    subp.add_argument('--install-prefix', type=Path, required=True)
    subp.add_argument('--generator', default='Unix Makefiles')
    subp.add_argument('--build-type', default='Debug')
    subp.add_argument('--cxxstd', default='20')
    subp.add_argument('--build-shared-libs', type=_str2bool, default=False)
    subp.set_defaults(func=_run_cmake_superproject_tests)

    subp = subparsers.add_parser('install-cmake-distro')
    subp.add_argument('--build-type', default='Debug')
    subp.set_defaults(func=_install_cmake_distro)

    subp = subparsers.add_parser('run-cmake-standalone-tests')
    subp.add_argument('--b2-distro', type=Path, required=True)
    subp.add_argument('--generator', default='Unix Makefiles')
    subp.add_argument('--build-type', default='Debug')
    subp.add_argument('--cxxstd', default='20')
    subp.add_argument('--build-shared-libs', type=_str2bool, default=False)
    subp.set_defaults(func=_run_cmake_standalone_tests)

    subp = subparsers.add_parser('run-cmake-add-subdirectory-tests')
    subp.add_argument('--generator', default='Unix Makefiles')
    subp.add_argument('--build-type', default='Debug')
    subp.add_argument('--build-shared-libs', type=_str2bool, default=False)
    subp.set_defaults(func=_run_cmake_add_subdirectory_tests)

    subp = subparsers.add_parser('run-cmake-find-package-tests')
    subp.add_argument('--cmake-distro', type=Path, required=True)
    subp.add_argument('--generator', default='Unix Makefiles')
    subp.add_argument('--build-type', default='Debug')
    subp.add_argument('--build-shared-libs', type=_str2bool, default=False)
    subp.set_defaults(func=_run_cmake_find_package_tests)

    subp = subparsers.add_parser('run-cmake-b2-find-package-tests')
    subp.add_argument('--cmake-distro', type=Path, required=True)
    subp.add_argument('--generator', default='Unix Makefiles')
    subp.add_argument('--build-type', default='Debug')
    subp.add_argument('--build-shared-libs', type=_str2bool, default=False)
    subp.set_defaults(func=_run_cmake_b2_find_package_tests)

    subp = subparsers.add_parser('run-b2-tests')
    subp.add_argument('--toolset', required=True)
    subp.add_argument('--cxxstd', default='20')
    subp.add_argument('--variant', default='debug,release')
    subp.add_argument('--stdlib', default='native')
    subp.add_argument('--address-model', default='64')
    subp.add_argument('--address-sanitizer', type=_str2bool, default=False)
    subp.add_argument('--undefined-sanitizer', type=_str2bool, default=False)
    subp.set_defaults(func=_run_b2_tests)

    args = parser.parse_args()
    args.func(**{k: v for k, v in vars(args).items() if k != 'func'})


if __name__ == '__main__':
    main()
