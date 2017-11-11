# Fuurin

Simple and fast [ZeroMQ](http://zeromq.org) communication library.

**TBD**

## Features

 - **TDB**


## Licenses

### MPL 2.0

The *fuurin* library is released under the terms of
[Mozilla Public License 2.0](https://www.mozilla.org/en-US/MPL/2.0/).
Please see the attached LICENSE file for further details.

### LGPL v3 plus a static linking exception

The [ZeroMQ](http://zeromq.org) library is released under
[GNU Lesser General Public License v3](http://www.gnu.org/licenses/lgpl.html),
plus a static linking exception.
Further details can be found at ZeroMQ [licensing](http://zeromq.org/area:licensing) page.

### Boost Software License 1.0

The [Boost Test Framework](https://github.com/boostorg/test), which is part of the
[Boost Library](http://www.boost.org), is distributed under the
[Boost Software License 1.0](http://www.boost.org/LICENSE_1_0.txt).
See the accompanying file [LICENSE_1_0.txt](vendor/boost/LICENSE_1_0.txt).

## How to build the library

[CMake](ttps://cmake.org) is used as project management tool to
both build and test this library.

```
$> mkdir build
$> cd build
$> cmake /path/to/fuurin/folder
$> make
$> make install
```

Library is installed on Unix systems to the default path `/usr/local`,
though a different PREFIX can be specified.
For example library can be installed to the path `/home/user/usr/local`
by setting the `DESTDIR` variable:

```
$> make DESTDIR=/home/user install
```

### How to make a debian package

Debian packages are generated through the following command:

```
$> cmake -D BUILD_DEB_PACKAGE=1 /path/to/fuurin/folder
$> make
$> (umask 022 && fakeroot make package)
```

Two packages are generated:

 * runtime: `libfuurin`
 * development: `libfuurin-dev`

Development package depends on runtime, so the latter one must be installed first
in order install the library headers.


### How to run tests

In order to run tests, they must be enabled first:


```
$> cmake -D BUILD_TESTS=1 /path/to/fuurin/folder
$> make
$> ctest -v
```


### Vendoring

External libraries used by *fuurin* are integrated through a vendoring approach.
For example, Boost unit test framework is added in this way:

 - Create an orphan branch where to store the external library

```
$> cd /path/to/fuurin/folder
$> git checkout --orphan vendor/boost
$> git reset
$> git ls-files -o | xargs rm
```

 - Extract a subset of Boost

```
$> cd /tmp
$> tar xf boost_1_65_1.tar.bz2
$> cd boost_1_65_1
$> ./bootstrap.sh
$> ./b2 tools/bcp
$> ./bin.v2/tools/bcp/.../bcp boost/test/unit_test.hpp boost/test/included/unit_test.hpp /path/to/fuurin/folder
```

 - Add Boost files to the orphan branch

```
$> cd /path/to/fuurin/folder
$> git add boost/ libs/
$> git ci -m"boost: import test framework version 1.65.1"
```

 - Checkout the Boost vendor branch into a subfolder of master branch

```
$> git checkout master
$> git merge -s ours --no-commit vendor/boost
$> git read-tree -u --prefix=vendor/boost vendor/boost
$> git commit
```

 - Update the Boost library and merge back into the master branch

```
$> git checkout vendor/boost
$> ... update library ...
$> git ci -m"boost: library updated"
$> git checkout master
$> git merge -s subtree -X subtree=vendor/boost vendor/boost
```


### Contribution guidelines

* Writing tests
* Code review
* Pull request
* Coding standard guidelines
