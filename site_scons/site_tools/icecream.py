# Copyright 2017 MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import SCons

import os
import re
import requests
import urllib.parse
import subprocess
import traceback

from pkg_resources import parse_version

icecream_version_min = '1.1rc2'


def generate(env):

    if not exists(env):
        return

    # Absoluteify, so we can derive ICERUN
    env['ICECC'] = env.WhereIs('$ICECC')

    if not 'ICERUN' in env:
        env['ICERUN'] = env.File('$ICECC').File('icerun')

    # Absoluteify, for parity with ICECC
    env['ICERUN'] = env.WhereIs('$ICERUN')

    # We can't handle sanitizer blacklist files, so disable icecc then, and just flow through
    # icerun to prevent slamming the local system with a huge -j value.
    if any(
            f.startswith("-fsanitize-blacklist=") for fs in ['CCFLAGS', 'CFLAGS', 'CXXFLAGS']
            for f in env[fs]):
        env['ICECC'] = '$ICERUN'

    # Make CC and CXX absolute paths too. It is better for icecc.
    env['CC'] = env.WhereIs('$CC')
    env['CXX'] = env.WhereIs('$CXX')

    action=None
    icecc_version_source=None
    # Make an isolated environment so that our setting of ICECC_VERSION in the environment
    # doesn't appear when executing icecc_create_env
    toolchain_env = env.Clone()
    # Make a predictable name for the toolchain
    icecc_version = env.Dir('$BUILD_ROOT/scons/icecc').File("icecc_version_file.tar.gz")

    def fetch_icecream_toolchain_url(target = None, source = None, env = None, url = None):
        try:
            print("URL: {}".format(url))
            response = requests.head(url,allow_redirects=True)
            if not response.ok:
                env.FatalError("error fetching url "+str(response))
            f = open(target[0].path,"w")
            f.write(response.url)
            f.close()
            return 0
            #file_size = int(response.headers['Content-length'])
        except:
            traceback.print_exec()
            return 1

    def download_icecream_toolchain(target = None, source = None, env = None):
        try:
            print("SOURCE {}".format(source[3]))
            url = open(source[3].path,"r").readline().rstrip()
            print("URL {}".format(url))
            response = requests.get(url)
            if not response.ok:
                raise Exception("error fetching url "+str(response))
            open(target[0].path,"wb").write(response.content)
            return 0
        except:
            traceback.print_exec()
            return 1

    if 'ICECC_VERSION' in env:
        parsed_url = urllib.parse.urlparse(env['ICECC_VERSION'])
        if parsed_url.scheme:
            if parsed_url.scheme == "file":
                if not parsed_url.netloc or parsed_url.netloc == 'localhost':
                    icecc_version_source=env.File(parsed_url.path)
                    action = SCons.Defaults.Copy('$TARGET','${SOURCES[3].abspath}')
                else:
                    env.FatalError("bad file:// url")
            elif parsed_url.scheme in [ "http", "https" ]:
                icecc_version_url = env.Dir('$BUILD_ROOT/scons/icecc').File("icecc_version_url")
                url = env['ICECC_VERSION']
                urlCommand = env.Command(
                    target = icecc_version_url,
                    source=[ ],
                    # use a lambda so that we can pass the url to the action
                    action = [ env.Action( lambda target, source, env : fetch_icecream_toolchain_url(target,source,env,url) ) ],
                )
                env.AlwaysBuild(icecc_version_url)
                icecc_version_source=icecc_version_url
                action = env.Action(download_icecream_toolchain)
                print("ALL SETUP")
            else:
                env.FatalError("url scheme "+ parsed_url.scheme +" is not handled the scons icecream support")
        else:
            icecc_version_source=env.File(env['ICECC_VERSION'])
            action = SCons.Defaults.Copy('$TARGET','${SOURCES[3].abspath}')
    elif toolchain_env.ToolchainIs('clang'):
        action = "${SOURCES[0]} --clang ${SOURCES[1].abspath} /bin/true $TARGET",
    else:
        action = "${SOURCES[0]} --gcc ${SOURCES[1].abspath} ${SOURCES[2].abspath} $TARGET",

    toolchain = env.Command(
        target=icecc_version,
        source=[
            '$ICECC_CREATE_ENV',
            '$CC',
            '$CXX',
            icecc_version_source,
        ],
        action=[ action ],
    )

    # Create an emitter that makes all of the targets depend on the
    # icecc_version_target (ensuring that we have read the link), which in turn
    # depends on the toolchain (ensuring that we have packaged it).
    def icecc_toolchain_dependency_emitter(target, source, env):
        env.Depends(target, toolchain)
        return target, source

    # Cribbed from Tool/cc.py and Tool/c++.py. It would be better if
    # we could obtain this from SCons.
    _CSuffixes = ['.c']
    if not SCons.Util.case_sensitive_suffixes('.c', '.C'):
        _CSuffixes.append('.C')

    _CXXSuffixes = ['.cpp', '.cc', '.cxx', '.c++', '.C++']
    if SCons.Util.case_sensitive_suffixes('.c', '.C'):
        _CXXSuffixes.append('.C')

    suffixes = _CSuffixes + _CXXSuffixes
    for object_builder in SCons.Tool.createObjBuilders(env):
        emitterdict = object_builder.builder.emitter
        for suffix in emitterdict.keys():
            if not suffix in suffixes:
                continue
            base = emitterdict[suffix]
            emitterdict[suffix] = SCons.Builder.ListEmitter([
                base,
                icecc_toolchain_dependency_emitter
            ])

    # Add ICECC_VERSION to the environment, pointed at the generated
    # file so that we can expand it in the realpath expressions for
    # CXXCOM and friends below.
    env['ICECC_VERSION'] = icecc_version

    if env.ToolchainIs('clang'):
        env['ENV']['ICECC_CLANG_REMOTE_CPP'] = 1
    else:
        env.AppendUnique(
            CCFLAGS=[
                '-fdirectives-only'
            ]
        )

    if 'ICECC_SCHEDULER' in env:
        env['ENV']['USE_SCHEDULER'] = env['ICECC_SCHEDULER']

    # Not all platforms have the readlink utility, so create our own
    # generator for that.
    def icecc_version_gen(target, source, env, for_signature):
        f = env.File('$ICECC_VERSION')
        if not f.islink():
            return f
        return env.File(os.path.realpath(f.abspath))

    env['ICECC_VERSION_GEN'] = icecc_version_gen

    def icecc_version_arch_gen(target, source, env, for_signature):
        if 'ICECC_VERSION_ARCH' in env:
            return "${ICECC_VERSION_ARCH}:"
        return str()
    env['ICECC_VERSION_ARCH_GEN'] = icecc_version_arch_gen

    env['ICECC_VERSION_ENV'] = 'ICECC_VERSION=${ICECC_VERSION_ARCH_GEN}${ICECC_VERSION_GEN}'

    # Make compile jobs flow through icecc
    env['CCCOM']    = '$( $ICECC_VERSION_ENV $ICECC $) ' + env['CCCOM']
    env['CXXCOM']   = '$( $ICECC_VERSION_ENV $ICECC $) ' + env['CXXCOM']
    env['SHCCCOM']  = '$( $ICECC_VERSION_ENV $ICECC $) ' + env['SHCCCOM']
    env['SHCXXCOM'] = '$( $ICECC_VERSION_ENV $ICECC $) ' + env['SHCXXCOM']

    # Make link like jobs flow through icerun so we don't kill the
    # local machine.
    #
    # TODO: Should we somehow flow SPAWN or other universal shell launch through
    # ICERUN to avoid saturating the local machine, and build something like
    # ninja pools?
    env['ARCOM'] = '$( $ICERUN $) ' + env['ARCOM']
    env['LINKCOM'] = '$( $ICERUN $) ' + env['LINKCOM']
    env['SHLINKCOM'] = '$( $ICERUN $) ' + env['SHLINKCOM']

    # Uncomment these to debug your icecc integration
    # env['ENV']['ICECC_DEBUG'] = 'debug'
    # env['ENV']['ICECC_LOGFILE'] = 'icecc.log'


def exists(env):

    icecc = env.get('ICECC', False)
    if not icecc:
        return False
    icecc = env.WhereIs(icecc)

    pipe = SCons.Action._subproc(env,
                                 SCons.Util.CLVar(icecc) + ['--version'], stdin='devnull',
                                 stderr='devnull', stdout=subprocess.PIPE)

    if pipe.wait() != 0:
        return False

    validated = False
    for line in pipe.stdout:
        line = line.decode('utf-8')
        if validated:
            continue  # consume all data
        version_banner = re.search(r'^ICECC ', line)
        if not version_banner:
            continue
        icecc_version = re.split('ICECC (.+)', line)
        if len(icecc_version) < 2:
            continue
        icecc_version = parse_version(icecc_version[1])
        needed_version = parse_version(icecream_version_min)
        if icecc_version >= needed_version:
            validated = True

    return validated
