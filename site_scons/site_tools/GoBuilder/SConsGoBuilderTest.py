import collections
import os.path

import unittest
import GoBuilder
import TestUnit


TESTDATA_DIRECTORY = os.path.join(os.path.dirname(__file__),'tests', 'testfiles')


class GoDummyEnv(collections.UserDict):
    def __init__(self,gotags=[],goos='linux',goarch='amd64',goversion='1.6',cgo=True):
        collections.UserDict.__init__(self)
        self['GOTAGS'] = gotags
        self['GOOS'] = goos
        self['GOARCH'] = goarch
        self['GOVERSION'] = goversion
        self['CGO_ENABLED'] = cgo

    # def subst(self, key):
    #     if key[0] == '$':
    #         return self[key[1:]]
    #     return key


class GoDummyNode(object):
    """
    Dummy test Node object to be passed into various GoBuilder methods
    # TODO: perhaps move to TestSCons ? It's pretty generic
    """
    def __init__(self,  name=None, search_result=(),contents=None):

        self.name = name
        self.search_result = tuple(search_result)
        self.contents = contents
        self.attributes=collections.UserDict()

    def __str__(self):
        return self.name

    # def Rfindalldirs(self, pathlist):
    #     return self.search_result + pathlist

    def get_contents(self):
        return self.contents


class TestImportParsing(unittest.TestCase):

    def test_single_line_imports(self):
        with open(os.path.join(TESTDATA_DIRECTORY,'test_single_line_imports.go'),'r') as sli:
            test_node=GoDummyNode(name='xyz.go', contents=sli.read())

            GoBuilder.parse_file(None, test_node)
            self.assertEqual(test_node.attributes.go_packages,['sli'])

    def test_multi_line_imports(self):
        test_node = GoDummyNode(name='xyz.go', contents="import (\n\t\"mli\"\n\t)")
        GoBuilder.parse_file(None,test_node)
        self.assertEqual(test_node.attributes.go_packages, ['mli'])

    def test_single_line_namespace_import(self):
        test_node=GoDummyNode(name='xyz.go', contents="package main\nimport abc \"slni\"\n")

        GoBuilder.parse_file(None, test_node)
        self.assertEqual(test_node.attributes.go_packages,['slni'])

    def test_multi_line_namespace_imports(self):
        test_node = GoDummyNode(name='xyz.go', contents="import (\n\tabc\"mlni\"\n\t)")
        GoBuilder.parse_file(None, test_node)
        self.assertEqual(test_node.attributes.go_packages, ['mlni'])


class TestBuildTagParsing(unittest.TestCase):

    @staticmethod
    def _load_single_build_node():
        with open(os.path.join(TESTDATA_DIRECTORY,'test_single_build_tag.go'),'r') as sli:
            test_node = GoDummyNode(name='xyz.go', contents=sli.read())
            GoBuilder.parse_file(None, test_node)
            return test_node

    def test_single_build_tag_parse(self):
        test_node = TestBuildTagParsing._load_single_build_node()

        self.assertEqual(test_node.attributes.go_build_statements,['wolf'])

    def test_single_build_tag_positive_eval(self):
        test_node = TestBuildTagParsing._load_single_build_node()

        testenv = GoDummyEnv()

        include_file = GoBuilder._eval_build_statements(testenv,test_node)
        self.assertFalse(include_file,'Failed testing negative for build tag wolf')

    def test_single_build_tag_negative_eval(self):
        test_node = TestBuildTagParsing._load_single_build_node()

        testenv = GoDummyEnv()
        testenv['GOTAGS'] = ['wolf']

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for build tag wolf')

    def test_goos_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go',contents='package testfiles\n\n// +build linux')
        testenv = GoDummyEnv()

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for build GOOS')

    def test_goos_negative_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build solaris')
        testenv = GoDummyEnv()

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for build GOOS')

    def test_goarch_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build amd64')
        testenv = GoDummyEnv()

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for build GOARCH')

    def test_goarch_negative_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build 386')
        testenv = GoDummyEnv()

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for build GOARCH')

    def test_cgo_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build cgo')
        testenv = GoDummyEnv()

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for build CGO')

    def test_cgo_negative_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build cgo')
        testenv = GoDummyEnv()
        testenv['CGO_ENABLED'] = False
        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for build CGO')

    def test_go_version_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build go1.6')
        testenv = GoDummyEnv()

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for build go version tag')

    def test_go_version_negative_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build go1.6')
        testenv = GoDummyEnv()
        # All go tags are defined up to the current version and so we must test
        # using a version lower than the required version to force a failure
        testenv['GOVERSION'] = '1.4'
        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for build go version tag')

    def test_go_single_line_anded_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf,bear')
        testenv = GoDummyEnv(gotags=['wolf','bear'])

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for single line comma ANDed build tags')

    def test_go_single_line_negative_anded_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf,bear')
        testenv = GoDummyEnv()
        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for single line comma ANDed build tags')

    def test_go_single_line_nanded_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build !wolf,bear')
        testenv = GoDummyEnv(gotags=['bear'])

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for single line comma ANDed build tags')

    def test_go_single_line_negative_nanded_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build !wolf,bear')
        testenv = GoDummyEnv()
        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for single line comma ANDed build tags')


    def test_go_single_line_ored_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf bear')
        testenv = GoDummyEnv(gotags=['wolf', 'bear'])

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for single line comma ORed build tags')

    def test_go_single_line_negative_ored_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf bear')
        testenv = GoDummyEnv()
        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for single line comma ORed build tags')

    def test_go_two_line_anded_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf\n// +build bear')
        testenv = GoDummyEnv(gotags=['wolf', 'bear'])

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for two line ANDed build tags')

    def test_go_two_line_negative_anded_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf\n// +build bear')
        testenv = GoDummyEnv()
        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for two line build tags')

    def test_go_two_line_ored_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf zebra\n// +build bear')
        testenv = GoDummyEnv(gotags=['wolf', 'bear','zebra'])

        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertTrue(include_file, 'Failed testing positive for two line space ORed build tags')

    def test_go_two_line_negative_ored_build_tag_eval(self):
        test_node = GoDummyNode(name='xyz.go', contents='package testfiles\n\n// +build wolf zebra\n// +build bear')
        testenv = GoDummyEnv()
        GoBuilder.parse_file(None, test_node)

        include_file = GoBuilder._eval_build_statements(testenv, test_node)
        self.assertFalse(include_file, 'Failed testing negative for two line space ORed build tags')


def suite():
    suite = unittest.TestSuite()
    tclasses = [
        TestImportParsing,
        TestBuildTagParsing,
               ]
    for tclass in tclasses:
        names = unittest.getTestCaseNames(tclass, 'test_')
        suite.addTests(list(map(tclass, names)))
    return suite


if __name__ == "__main__":
    TestUnit.run(suite())
