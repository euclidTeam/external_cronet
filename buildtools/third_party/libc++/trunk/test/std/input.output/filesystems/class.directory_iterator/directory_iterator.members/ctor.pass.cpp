//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03

// <filesystem>

// class directory_iterator

// explicit directory_iterator(const path& p);
// directory_iterator(const path& p, directory_options options);
// directory_iterator(const path& p, error_code& ec);
// directory_iterator(const path& p, directory_options options, error_code& ec);

#include "filesystem_include.h"
#include <type_traits>
#include <set>
#include <cassert>

#include "assert_macros.h"
#include "test_macros.h"
#include "filesystem_test_helper.h"

using namespace fs;

static void test_constructor_signatures()
{
    using D = directory_iterator;

    // explicit directory_iterator(path const&);
    static_assert(!std::is_convertible<path, D>::value, "");
    static_assert(std::is_constructible<D, path>::value, "");
    static_assert(!std::is_nothrow_constructible<D, path>::value, "");

    // directory_iterator(path const&, error_code&)
    static_assert(std::is_constructible<D, path,
        std::error_code&>::value, "");
    static_assert(!std::is_nothrow_constructible<D, path,
        std::error_code&>::value, "");

    // directory_iterator(path const&, directory_options);
    static_assert(std::is_constructible<D, path, directory_options>::value, "");
    static_assert(!std::is_nothrow_constructible<D, path, directory_options>::value, "");

    // directory_iterator(path const&, directory_options, error_code&)
    static_assert(std::is_constructible<D, path, directory_options,
        std::error_code&>::value, "");
    static_assert(!std::is_nothrow_constructible<D, path, directory_options,
        std::error_code&>::value, "");

}

static void test_construction_from_bad_path()
{
    static_test_env static_env;
    std::error_code ec;
    directory_options opts = directory_options::none;
    const directory_iterator endIt;

    const path testPaths[] = { static_env.DNE, static_env.BadSymlink };
    for (path const& testPath : testPaths)
    {
        {
            directory_iterator it(testPath, ec);
            assert(ec);
            assert(it == endIt);
        }
        {
            directory_iterator it(testPath, opts, ec);
            assert(ec);
            assert(it == endIt);
        }
        {
            TEST_THROWS_TYPE(filesystem_error, directory_iterator(testPath));
            TEST_THROWS_TYPE(filesystem_error, directory_iterator(testPath, opts));
        }
    }
}

static void access_denied_test_case()
{
    using namespace fs;
#ifdef _WIN32
    // Windows doesn't support setting perms::none to trigger failures
    // reading directories; test using a special inaccessible directory
    // instead.
    const path testDir = GetWindowsInaccessibleDir();
    if (testDir.empty())
        return;
#else
    scoped_test_env env;
    path const testDir = env.make_env_path("dir1");
    path const testFile = testDir / "testFile";
    env.create_dir(testDir);
    env.create_file(testFile, 42);

    // Test that we can iterator over the directory before changing the perms
    {
        directory_iterator it(testDir);
        assert(it != directory_iterator{});
    }
    // Change the permissions so we can no longer iterate
    permissions(testDir, perms::none);
#endif

    // Check that the construction fails when skip_permissions_denied is
    // not given.
    {
        std::error_code ec;
        directory_iterator it(testDir, ec);
        assert(ec);
        assert(it == directory_iterator{});
    }
    // Check that construction does not report an error when
    // 'skip_permissions_denied' is given.
    {
        std::error_code ec;
        directory_iterator it(testDir, directory_options::skip_permission_denied, ec);
        assert(!ec);
        assert(it == directory_iterator{});
    }
}


static void access_denied_to_file_test_case()
{
    using namespace fs;
    scoped_test_env env;
    path const testFile = env.make_env_path("file1");
    env.create_file(testFile, 42);

    // Change the permissions so we can no longer iterate
    permissions(testFile, perms::none);

    // Check that the construction fails when skip_permissions_denied is
    // not given.
    {
        std::error_code ec;
        directory_iterator it(testFile, ec);
        assert(ec);
        assert(it == directory_iterator{});
    }
    // Check that construction still fails when 'skip_permissions_denied' is given
    // because we tried to open a file and not a directory.
    {
        std::error_code ec;
        directory_iterator it(testFile, directory_options::skip_permission_denied, ec);
        assert(ec);
        assert(it == directory_iterator{});
    }
}

static void test_open_on_empty_directory_equals_end()
{
    scoped_test_env env;
    const path testDir = env.make_env_path("dir1");
    env.create_dir(testDir);

    const directory_iterator endIt;
    {
        std::error_code ec;
        directory_iterator it(testDir, ec);
        assert(!ec);
        assert(it == endIt);
    }
    {
        directory_iterator it(testDir);
        assert(it == endIt);
    }
}

static void test_open_on_directory_succeeds()
{
    static_test_env static_env;
    const path testDir = static_env.Dir;
    std::set<path> dir_contents(static_env.DirIterationList.begin(),
                                static_env.DirIterationList.end());
    const directory_iterator endIt{};

    {
        std::error_code ec;
        directory_iterator it(testDir, ec);
        assert(!ec);
        assert(it != endIt);
        assert(dir_contents.count(*it));
    }
    {
        directory_iterator it(testDir);
        assert(it != endIt);
        assert(dir_contents.count(*it));
    }
}

static void test_open_on_file_fails()
{
    static_test_env static_env;
    const path testFile = static_env.File;
    const directory_iterator endIt{};
    {
        std::error_code ec;
        directory_iterator it(testFile, ec);
        assert(ec);
        assert(it == endIt);
    }
    {
        TEST_THROWS_TYPE(filesystem_error, directory_iterator(testFile));
    }
}

static void test_open_on_empty_string()
{
    const path testPath = "";
    const directory_iterator endIt{};

    std::error_code ec;
    directory_iterator it(testPath, ec);
    assert(ec);
    assert(it == endIt);
}

static void test_open_on_dot_dir()
{
    const path testPath = ".";

    std::error_code ec;
    directory_iterator it(testPath, ec);
    assert(!ec);
}

static void test_open_on_symlink()
{
    static_test_env static_env;
    const path symlinkToDir = static_env.SymlinkToDir;
    std::set<path> dir_contents;
    for (path const& p : static_env.DirIterationList) {
        dir_contents.insert(p.filename());
    }
    const directory_iterator endIt{};

    {
        std::error_code ec;
        directory_iterator it(symlinkToDir, ec);
        assert(!ec);
        assert(it != endIt);
        path const& entry = *it;
        assert(dir_contents.count(entry.filename()));
    }
    {
        std::error_code ec;
        directory_iterator it(symlinkToDir,
                              directory_options::follow_directory_symlink, ec);
        assert(!ec);
        assert(it != endIt);
        path const& entry = *it;
        assert(dir_contents.count(entry.filename()));
    }
}

int main(int, char**) {
    test_constructor_signatures();
    test_construction_from_bad_path();
    access_denied_test_case();
    access_denied_to_file_test_case();
    test_open_on_empty_directory_equals_end();
    test_open_on_directory_succeeds();
    test_open_on_file_fails();
    test_open_on_empty_string();
    test_open_on_dot_dir();
    test_open_on_symlink();

    return 0;
}
