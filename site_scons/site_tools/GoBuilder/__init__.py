# -*- mode:python; coding:utf-8; -*-

#  A SCons tool to enable compilation of go in SCons.
#
# Copyright © 2016 William Deegan
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


import SCons.Tool
from SCons.Builder import Builder
from SCons.Action import Action, _subproc
from SCons.Scanner import Scanner, FindPathDirs
from SCons.Defaults import ObjSourceScan
import SCons.Subst
import os.path
import os
import re
import string
import pdb
import subprocess


go_debug = True
print "XXX LOADING GOBUILDER\n"

# Handle multiline import statements
m_import = re.compile(r'import\s*(?P<paren>\()\s*(?P<package_names>[^\(]*?)(\))|import\s*(?P<namespace>[^\"]*)?(?P<quote>\")(?P<package_name>.*?)(\")', re.MULTILINE)

# Handle +build statements
m_build = re.compile(r'\/\/\s*\+build\s(.*)')

# TODO: Determine if lists below include all those supported by both gccgo and google go
#       or just google go.  If just google go, obtain complete list and add to below
#       or perhaps after we determine which compiler we're using use the appropriate list
#       though in theory, this is just to validate settings for the GOOS AND GOARCH environment
#       variables.
# list of valid goos and gooarch from
# https://github.com/golang/go/blob/master/src/go/build/syslist.go
# Need to include license statement from
# https://github.com/golang/go/blob/master/LICENSE
_goosList = "android darwin dragonfly freebsd linux nacl netbsd openbsd plan9 solaris windows".split()
_goarchList = "386 amd64 amd64p32 arm armbe arm64 arm64be ppc64 ppc64le mips mipsle mips64 mips64le mips64p32 mips64p32le ppc s390 s390x sparc sparc64".split()

_goVersionTagsList = ['go1.%d'%v for v in xrange(1,8)]

# TODO: construct list of legal combinations
#  Use: https://golang.org/doc/install/source#environment for current list.  (may vary by version of google go compiler)
# android	arm
# darwin	386
# darwin	amd64
# darwin	arm
# darwin	arm64
# dragonfly	amd64
# freebsd	386
# freebsd	amd64
# freebsd	arm
# linux	386
# linux	amd64
# linux	arm
# linux	arm64
# linux	ppc64
# linux	ppc64le
# linux	mips64
# linux	mips64le
# netbsd	386
# netbsd	amd64
# netbsd	arm
# openbsd	386
# openbsd	amd64
# openbsd	arm
# plan9	386
# plan9	amd64
# solaris	amd64
# windows	386
# windows	amd64


def check_go_file(node,env):
    """
    Check if the node is either ready to scan now, or should be scanned
    :param node: File/Node to be scanned
    :param env: Environment()
    :return: Boolean value indicated whether to scan this file
    """

    process_file = True

    file_abspath = node.abspath

    if not include_go_file(env,node):
        process_file = False

    if go_debug: print "check_go_File:%s  Process:%s"%(file_abspath,process_file)

    return process_file


def is_not_go_standard_library(env, packagename):
    """ Check with go if the imported module is part of
    Go's standard library"""

    #TODO: What to do when there's a system package and a package in the project with the same name

    return packagename not in env['GO_SYSTEM_PACKAGES']


def parse_file(env,node):
    """
    Parse the file and get import and // +build statements
    :param env:
    :param node:
    :return:
    """

    packages = []
    build_statements = []
    content = node.get_contents()

    for b in m_build.finditer(content):
        if b.group(1):
            build_statements.append(b.group(1))

    if len(build_statements) > 0:
        if go_debug: print("+build statements (file:%s):%s"%(node.abspath,build_statements))

    for m in m_import.finditer(content):
        match_dict = m.groupdict()
        if match_dict['paren']:
            # imports = [x.strip(' "\t') for x in m.group(2).splitlines()]
            lines = match_dict['package_names'].splitlines()
            imports = []
            for l in lines:
                l = l.strip(' "\t')
                if l == '': continue

                # Handle multi-line imports with namespaces by striping namespace
                quote_pos = l.find('"')
                if quote_pos != -1:
                    l = l[quote_pos+1:]
                imports.append(l)

            if go_debug:
                print("Import() ["+ ",".join(imports)+"]")

            # Strip empty strings from array. This may happen due to newlines
            # imports = [i for i in imports if i != '']
            packages.extend(imports)
        else:
            # single line import statements
            if go_debug:
                print "Import \"\"", m.group(5)
            if go_debug and match_dict['namespace']:
                print "NAMESPACE:%s"%match_dict['namespace']
            packages.append(match_dict['package_name'])

    node.attributes.go_packages = packages
    node.attributes.go_build_statements = build_statements


def include_go_file(env,file):
    """
    Use file name (and also scan for +build statements in file to determine
    if this file should be compiled and/or added to dependency tree
    :param env: Environment being used to compile this file
    :param file: The file in question
    :return: Boolean indicating if this file should be included
    """

    include_file = True

    # First filter based on file name
    file_parts = file.name.split('.')[0].split('_')
    if go_debug: print "parts:%s"%file_parts

    if file_parts[-1] == 'test':
        include_file = False
    elif file_parts[-1] != env['GOOS'] and file_parts[-1] in _goosList:
        include_file = False
    elif file_parts[-1] != env['GOARCH'] and file_parts[-1] in _goarchList:
        include_file = False
    elif file_parts[-1] == env['GOARCH'] and file_parts[-2] != env['GOOS'] and file_parts[-2] in _goosList:
        include_file = False

    if include_file:
        # Cheap tests are done, now parse file and get build tags and import statements
        parse_file(env,file)
        include_file = _eval_build_statements(env, file)

    if not include_file:
        if go_debug: print "Rejecting: %s"%file.name

    return include_file


def expand_go_packages_to_files(env, gopackage, path):
    """
    Use GOPATH and package name to find directory where package is located and then scan
    the directory for go packages. Return complete list of found files
    TODO: filter found files by normal go file pruning (matching GOOS,GOOARCH,
         and //+build statements in files
    :param env:
    :param gopackage:
    :param path:
    :return:
    """
    go_files = []

    for p in path:
        if go_debug: print "Path:%s/src/%s"%(p,gopackage)
        go_files = p.glob("src/%s/*.go"%gopackage)
        count=0
        for f in go_files:
            # print "(%4d) File:%s  Test:%s"%(count,f.abspath,('_test' in f.abspath))
            count += 1
        if len(go_files) > 0:
            # If we found packages files in this path, the don't continue searching GOPATH
            break

    # print "filtering test files"
    # for f in go_files:
    #     print "%s : %s"%(type(f), f)
    try:
        go_files = [ f for f in go_files if include_go_file(env,f)]
    except TypeError as e:
        if go_debug: print "TypeError is %s %s"%(e,f)
    # print "done filtering test files"
    count = 0
    # for f in go_files:
    #     print "X(%4d) File:%s  Test:%s" % (count, f.abspath, ('_test' in f.abspath))
    #     count += 1

    return go_files


def _eval_build_statement(env,statement,all_tags):
    """
    Process a single build statements arguments
    :param env:
    :param statement:
    :return: Boolean indicating if satisfied
    """

    if statement is None:
        import pdb; pdb.set_trace()

    inverted = statement[0] == '!'
    retval = False
    if inverted:
        statement = statement[1:]

    if statement in all_tags:
        retval = True

    if inverted:
        return not retval
    else:
        return retval


def _get_all_tags(env):

    tags = []
    tags.extend(env['GOTAGS'])
    tags.append(env['GOOS'])
    tags.append(env['GOARCH'])

    if env.get('CGO_ENABLED',False):
        tags.append('cgo')

    go_version = int(env['GOVERSION'].split('.')[1]) # strip '1.' and '.x' and just get middle version
    tags.extend(_goVersionTagsList[0:go_version])

    # print("ALL_TAGS:%s"%tags)

    return tags


def _eval_build_statements(env,node):
    """
    Process // +build statements in source files as follows:
    If more than one line of +build statements, then each lines logic is evaluated and
    then AND'd with all other lines.
    If there is a comma between two items on a build line that indicates AND'ing them,
    otherwise all items space separated are OR'd.
    :param env:
    :param node: The node being evaluated. Used for debug messaging.
    :return: Boolean indicating whether the constraints are satisfied.
    """

    # if node.name.endswith('c.go'): pdb.set_trace()

    # A list of build statements, each one representing a line extracted from the source file
    build_statements = node.attributes.go_build_statements

    if not build_statements:
        return True

    # Split each line into a list of space separated parts
    build_statement_parts = [bs.split(' ') for bs in build_statements]

    all_tags = _get_all_tags(env)
    retval = True

    for line_parts in build_statement_parts:
        line = False
        for p in line_parts:
            lpbool = [_eval_build_statement(env, pp, all_tags) for pp in  p.split(',')]
            prv = reduce(lambda x, y: x and y,lpbool)
            if go_debug: print "PARTS:%s =>%s [%s]"%(p,prv,lpbool)
            line = line or prv
        retval = retval and line

        if go_debug: print("_eval_build_statements:File:%s Process:%s"%(node.abspath,retval))

    return retval


def imported_modules(node, env, path):
    """
    Scan file for +build statements and also for import statements.
    Store import statements on the node in node.attributes.go_imports
    :param node: The file we are looking at
    :param env:  Environment()
    :param path:
    :return:
    """
    """ Find all the imported modules. """

    # Does the node exist yet?
    if not os.path.isfile(str(node)):
        return []

    deps = []

    # print "Paths to search: %s"%path
    if go_debug: print("Node PATH      : %s (CGF:%s)"%(node.path,hasattr(node.attributes,'go_packages')))

    packages = node.attributes.go_packages
    if packages:
        if go_debug: print "packages:%s"%packages

    fixed_packages = []
    # Add filter to handle package renaming as such
    # import m "lib/math"         (where the contents of lib/math are accessible via m.SYMBOL
    for p in packages:
        quote_pos = p.find('"')
        if quote_pos != -1:
            # Import statement has package name, remove it.
            p = p[quote_pos+1:]
        fixed_packages.append(p)

    import_packages = [go_package for go_package in fixed_packages if is_not_go_standard_library(env, go_package)]

    for go_package in import_packages:
        deps += expand_go_packages_to_files(env,go_package,path)

    return deps


is_String = SCons.Util.is_String
is_List = SCons.Util.is_List


def _create_env_for_subprocess(env):
    """ Create string-ified environment to pass to subprocess
    This logic is mostly copied from SCons.Action._subproc
    """

    # Ensure that the ENV values are all strings:
    new_env = dict()
    for key, value in env['ENV'].items():
        if is_List(value):
            # If the value is a list, then we assume it is a path list,
            # because that's a pretty common list-like value to stick
            # in an environment variable:
            value = SCons.Util.flatten_sequence(value)
            new_env[key] = os.pathsep.join(map(str, value))
        else:
            # It's either a string or something else.  If it's a string,
            # we still want to call str() because it might be a *Unicode*
            # string, which makes subprocess.Popen() gag.  If it isn't a
            # string or a list, then we just coerce it to a string, which
            # is the proper way to handle Dir and File instances and will
            # produce something reasonable for just about everything else:
            new_env[key] = str(value)
    return new_env


def _get_system_packages(env):
    """
    Determine list of system packages by subtracting the list of project packages from
    the list of all packages known within the scope of the current project
    :param env:
    :return:
    """

    # use
    subp_env = _create_env_for_subprocess(env)

    all_packages = set(subprocess.check_output([env.subst('$GO'), "list", "..."], env=subp_env).split())

    proj_packages = set(subprocess.check_output([env.subst('$GO'), "list", "./..."], env=subp_env).split())

    system_packages = all_packages - proj_packages

    # print "All PACKAGES: %s" % all_packages
    # print "    PACKAGES: %s" % proj_packages
    # print "GlobPackages: %s" % system_packages
    # print "PACKAGES: %s" % [s for s in stdout]
    env['GO_SYSTEM_PACKAGES'] = list(system_packages)


def _get_go_version_vendor(env):
    """
    Extract go version information from running 'go version'

    Seems to come in two flavors:
    gcc go    : go version go1.4.2 gccgo (GCC) 5.3.0 linux/amd64
    google go : go version go1.2.1 linux/amd64
    :param env:
    :return:
    """
    subp_env = _create_env_for_subprocess(env)

    version_parts = subprocess.check_output([env.subst('$GO'), "version"], env=subp_env).split()

    go_version = version_parts[2][2:]
    (go_os,go_arch) = version_parts[-1].split('/')

    print("GO VERSION:%s GOOS:%s GOARCH:%s"%(go_version, go_os,go_arch))
    env['GOHOSTOS'] = go_os
    env['GOHOSTARCH'] = go_arch
    env['GOVERSION'] = go_version


def _get_go_env_values(env):
    subp_env = _create_env_for_subprocess(env)

    go_env_values = subprocess.check_output([env.subst('$GO'), "env"], env=subp_env).split(b'\n')

    for var in go_env_values:
        if not var: continue
        # print("Line:%s"%var)
        (variable,value) = var.split('=',1)
        if (variable.startswith('GO') or variable == 'CGO_ENABLED') and variable not in ('GOPATH'):
            # TODO: Decide if we stomp on any existing values here.
            env[variable] = value[1:-1]
            if go_debug: print("Setting: %s = %s"%(variable,env[variable]))


def _go_emitter(target, source, env):
    if len(source) == 1:
        target.append(str(source).replace('.go',''))
    return target, source


def _go_tags(target, source, env, for_signature):
    """
    Produce go tag arguments for command line
    :param env:
    :return:
    """
    if go_debug: print("ForSig:%s TAGS:%s"%(for_signature,env['GOTAGS']))
    retval = ''
    if env['GOTAGS']:
        retval = ' -tags "$GOTAGS" '
    return retval


def generate(env):
    go_suffix = '.go'

    go_binaries = ['go','gccgo-go','gccgo']

    # If user as set GO, honor it, otherwise try finding any of the expected go binaries
    env['GO'] = env.get('GO',False) or env.Detect(['go','gccgo-go'])  or "ECHO"

    go_path = env.WhereIs(env['GO'])
    # Typically this would be true, but it shouldn't need to be set if
    # We're using the "go" binary from either google or gcc go.
    # env['ENV']['GOROOT'] = os.path.dirname(os.path.dirname(go_path))
    # GOROOT points to where go environment is setup.
    # Only need to set GOROOT if GO is not in your path..  check if there's a dirsep in the GO path
    # gcc go has implicit GOROOT and it's where --prefix pointed when built.
    # Only honor user provide GOROOT. otherwise don't specify.
    env['ENV']['GOPATH'] = env['ENV'].get('GOPATH',env.get('GOPATH','.'))

    # Populate GO_SYSTEM_PACKAGES
    # TODO: Add documentation for GO_SYSTEM_PACKAGES
    _get_system_packages(env)

    _get_go_version_vendor(env)

    _get_go_env_values(env)

    goSuffixes = [".go"]

    # compileAction = Action("$GOCOM","$GOCOMSTR")

    linkAction = Action("$GOLINK","$GOLINKSTR")

    goScanner = Scanner(function=imported_modules,
                        scan_check=check_go_file,
                        name="goScanner",
                        skeys=goSuffixes,
                        path_function=FindPathDirs('GOPATH'),
                        recursive=True)

    goProgram = Builder(action=linkAction,
                        prefix="$PROGPREFIX",
                        suffix="$PROGSUFFIX",
                        source_scanner=goScanner,
                        src_suffix=".go",
                        src_builder="goObject")
    env["BUILDERS"]["goProgram"] = goProgram

    # goLibrary = SCons.Builder.Builder(action=SCons.Defaults.ArAction,
    #                                   prefix="$LIBPREFIX",
    #                                   suffix="$LIBSUFFIX",
    #                                   src_suffix="$OBJSUFFIX",
    #                                   src_builder="goObject")
    # env["BUILDERS"]["goLibrary"] = goLibrary

    # goObject = Builder(action=compileAction,
    #                    # emitter=addgoInterface,
    #                    # emitter=_go_emitter,
    #                    prefix="$OBJPREFIX",
    #                    suffix="$OBJSUFFIX",
    #                    src_suffix=goSuffixes,
    #                    source_scanner=goScanner)
    # env["BUILDERS"]["goObject"] = goObject

    # initialize GOOS and GOARCH
    # TODO: Validate the values of each to make sure they are valid for GO.
    # TODO: Document all the GO variables.

    env['GOOS']   = env.get('GOOS',env['GOHOSTOS'])
    env['GOARCH'] = env.get('GOARCH',env['GOHOSTARCH'])

    env['_go_tags_flag'] = _go_tags

    #env["GOFLAGS"] = env["GOFLAGS"] or None

    if 'GOTAGS' not in env:
        env['GOTAGS'] = []

    env['GOCOM'] = '$GO build -o $TARGET ${_go_tags_flag} $GOFLAGS $SOURCES'
    env['GOCOMSTR'] = '$GOCOM'
    env['GOLINK'] = '$GO build -o $TARGET $_go_tags_flag $GOFLAGS $SOURCES'
    env['GOLINKSTR'] = '$GOLINK'



    # import SCons.Tool
    # static_obj, shared_obj = SCons.Tool.createObjBuilders(env)
    # static_obj.add_action(go_suffix, compileAction)
    # shared_obj.add_action(go_suffix, compileAction)
    #
    # static_obj.add_emitter(go_suffix, SCons.Defaults.StaticObjectEmitter)
    # shared_obj.add_emitter(go_suffix, SCons.Defaults.SharedObjectEmitter)

def exists(env):
    print('GOBUILDER exists')
    return env.Detect("go") or env.Detect("gnugo")
