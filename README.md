# Fuurin

Simple and fast [ZeroMQ](http://zeromq.org) communication library.

It provides few easy to use C++ classes in order to connect different parties together.
There are also C++ bindings to some ZMQ sockets and features, which are used by the library
components themselves.

## Table of Contents
1. [Features](#Features)
2. [Architecture](#Architecture)
3. [Licenses](#Licenses)
4. [Guidelines](#Guidelines)

## Features

 * ZMQ Sockets:
    - ZMQ C++ bindings with RAII design and exception safe code.
    - Leverage of ZMQ thread safe sockets, like RADIO/DISH, CLIENT/SERVER.
    - Multipart messages can be used with ZMQ thread safe sockets.
    - Polling of ZMQ sockets made easy: can integrate with C++ range based loops, easy open/close sockets, etc..
    - Multicore friendly, due to the usage of ZMQ and Boost::ASIO.
    - Logging aware library: levels, customization and performance.
    - Time elapsed measurement for timeouts.
    - Endianess management for integer types.
    - Timers can be polled like they were sockets.

 * Network Classes:
    - Microservices mini framework, based on workers and brokers.
    - Connection management between worker and broker.
    - Redesigned and extended [ZMQ Clone Pattern](https://zguide.zeromq.org/docs/chapter5/#Reliable-Pub-Sub-Clone-Pattern).
    - Late joining management in publish/subscribe patterns.
    - Retry and keep alive to check for liveness.
    - Eventually consistent and coherent distributed system, for high availability.
    - Universally unique identifier support (UUID).
    - Reliability by means of supported redundant connections.

(_TODO: add references to classes where possibile._)


## Architecture

### Components

This library provides two main components which are *worker* and *broker*. The communication
between them is based on ZMQ thread-safe sockets [Radio/Dish](https://rfc.zeromq.org/spec/48/) and
[Client/Server](https://rfc.zeromq.org/spec/41/), as shown in the following diagram:

![Worker Broker Connections](http://www.plantuml.com/plantuml/png/VP11QyCm38Nl_XMYE_iVZ5BQ74S9ow6KTGUJYCRKaOpZh9In_xuejKctGcuMrlVqtjlqqOGuT0vgxZmJKbJAc_fYpWZRm1SCyF9cpstStGp1jmA0UHLM1JhxXU5s8lXuDutbpnMO7hR5yG7x3rLaVDzo5AZ2CFA9glOBL65xRsBT2ZM-stofV6H-PlS73hFx8ph7rsV_IGfEb9DCgeVaFuFFsCxPyILWJdC7g_qYc5eIBkT91yi_d0IH4dN3Lz9hCQH4MmzUhwMeQNgzVq-pAEWW2h9Gb4fja9gXSxy0)

Basic operations:
  - **Dispatch**: A worker *dispatches* a message, i.e. a *topic*, to any connected broker.
    Every message is marked with a *sequence number*, at worker's side.
    Depending on the underlying ZMQ socket transport, a topic might be dispatched to more than
    a single broker.

    ZMQ sockets: [Radio/Dish](https://rfc.zeromq.org/spec/48/).

  - **Delivery**: A broker *delivers* data to any connected workers, whenever it receives a *topic*.
    A broker filters out every message marked with a sequence number that is less or equal to the
    last seen sequence number, for every single worker. Every worker does such filtering as well.
    Every received topic is locally stored, at broker side.

    ZMQ sockets: [Radio/Dish](https://rfc.zeromq.org/spec/48/).

  - **Synchronize**: A worker *synchronizes* with lastest broker's *snapshot*.
    A broker replies with its snapshot, before serving any other dispatched topics.
    Returned topics are the only ones marked as *state*, instead *event* ones are just delivered.

    ZMQ sockets: [Client/Server](https://rfc.zeromq.org/spec/41/).


### Usage

Upon instantiation of worker and broker classes, an unique UUID is assigned in order to
distinguish them. Every components will connect/bind to the speicified ZMQ endopoints.
After starting both components, an asynchronous connection is set up. Data will begin to
flow as soon as the connection gets ready. Auto reconnection to the broker is automatically
performed by the worker, in case the former disappears. At any time, a worker can synchronize
with its connected broker.

![Usage_Diagram](http://www.plantuml.com/plantuml/png/ZLDDJm8n4BttLpJnB8aSITOWS3J1eCPpqnrWmZAjRPTa_VMsx62eZ8ctTZxUctbzdSTaGkgFdRRkw1q19QMQHh-Mi6uQfOnDBkXbXoNbSnGjUaD9VxXmWF1GnHR1vPY-UyRTFYq7GqgDdVh-yTAWPoEwudj9SH5de3tJuiaak7HTLpFBJ3yHkPuiA8vK92z8Ev5ZJHqIg1P-SuoR3sJtmH5-cOIEYWRo3hbEXD_0PmynBo6Cp6q_st5Sd7zz4E4Ni4FYXWfv0xuRHAJ9HByvZpRcqqG4hIiaZ6MsHmUfaes7bn-ojnPYF4lwxjjS7ekKWeEcjt9mebflXT6RPFVYT2ley0HXa4OPDvFUa3Dy_n_Rz8hiHWh-Eix_xPgSb4svtHPNohwlYXd5GojQU0xKvl-ilW40)

### Configurations

Some examples of possible configurations, by putting pieces together.

#### Sharing of State among Services
Every service can export and distribute its data to others and a central broker can manage the
accesses to shared state.

![Conf_Services](http://www.plantuml.com/plantuml/png/XT2noeCm4C3n_PuY-CqEwluXRKU7GWTnFEBLIZMHUBQKqdVlaLYqfT1C_y2Fov5yP7GyzHqCgC_Oa8eEZ4oH-YlQviJR6nfr1oMdDKpkYDeJwuJWg3Qx_Kf-ki9YFRDgpHu0slQ3DMHOff6xj9eIByjaXXLrdRr-SMbmwI-N1PUzEv07ucc8__tg86FYq23obI3x-akPB9akcK5EffPVUm80)

#### Master and Replica
Whenever a service publishes its data, it can dispatch it to both a master and a replica.
Switchover between master and replica, or activation of services, are beyond the scope of
this library.

![Conf_Replica](http://www.plantuml.com/plantuml/png/bP3Dgi8m48NtynH3xxgBsmUGeYuhY2vAbqCwrc2QX2IrYFZkfZ-WTgNPdV1nFixaFf0BNQl0ahXGmvZio0Ts2VuLiZc7pOqqtW7Zaph-dqX4vXYCumJ9utgx_tz3bs1Xg9wvweDxmCjuOAkae1-KsPVARA44OLfhDEiG2zbTfPWgovchM2dJ0vIOTZPTZUk6K9jUJp436AUaCPQGiww7upq1)

#### Redundancy for Cabling
It's possible to configure multiple paths from worker to broker, using more than just one network.
Messages will be filtered out by both worker and broker, using sequence numbers.

![Conf_Cables](http://www.plantuml.com/plantuml/png/NT31QeGm40RW-pn5i6SFvW6Aj8LU91Hw48z3d5en9XB75LdstJVHB8Wv3JzVXfyfPqRFosW09jG3TYIoNqQcJBnLVVVFdnjQSGSHNc-P_1_gdJWV2CxYu-ld9A-kSjWcrfpP0q2xSNAMB8Tjv6-zFlRLYJLaZ5j16xUq8bF4g_D3iHDL9FFjSRi8UGXv5b2BF7yFtq0LSOgTNva49UCK8mWbBx9EMP8fWv9i6s_s1000)

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

### Apache License 2.0

The [Google Benchmark](https://github.com/google/benchmark) library is distributed under
[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
See the accompanying file [LICENSE](vendor/google/benchmark/LICENSE).


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
$> cmake -D BUILD_DEB_PACKAGE=ON /path/to/fuurin/folder
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


### How to enable sanitizers

Sanitizers can enabled with some cmake options:

  - Address Sanitizer: cmake -D ENABLE_ASAN=ON ...
  - Thread Sanitezer: cmake -D ENABLE_TSAN=ON ...
  - Memory Sanitezer: cmake -D ENABLE_MSAN=ON ...
  - Undefined Behavior Sanitizer: cmake -D ENABLE_UBSAN=ON ...


### How to compile with clang


```
$> cmake -D CMAKE_C_COMPILER=/usr/bin/clang -D CMAKE_CXX_COMPILER=/usr/bin/clang++ /path/to/fuurin/folder
$> make
```


### Vendoring

External libraries used by *fuurin* are integrated through a git subtree approach.
For example, Boost unit test framework may be added in this manual way:

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
$> ./bin.v2/tools/bcp/.../bcp \
    LICENSE_1_0.txt \
    boost/test/unit_test.hpp \
    boost/test/included/unit_test.hpp \
    boost/test/data/test_case.hpp \
    libs/test/test/test-organization-ts/datasets-test/* \
    libs/test/example/* \
    boost/mpl/list.hpp \
    boost/scope_exit.hpp \
    boost/typeof/incr_registration_group.hpp \
    boost/uuid/uuid.hpp \
    boost/uuid/uuid_generators.hpp \
    boost/uuid/uuid_io.hpp \
    boost/uuid/uuid_hash.hpp \
    boost/asio.hpp \
    /path/to/fuurin/folder
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
$> git merge --allow-unrelated-histories -s ours --no-commit vendor/boost
$> git read-tree -u --prefix=vendor/boost vendor/boost
$> git commit
```

 - Update the Boost library and merge back into the master branch

```
$> ## folder must be free of untracked files
$> git checkout vendor/boost --
$> ## optional clear all tracked files
$> git ls-files | xargs git rm
$> ## update library by adding new untracked files
$> git ls-files -o | xargs git add
$> git ci -m"boost: library updated"
$> git checkout master
$> git merge -s subtree -X subtree=vendor/boost vendor/boost
```


## Guidelines

* Contribution: *TBD*
* Writing tests: *TBD*
* Code review: *TDB*
* Pull request: *TDB*
* Coding standard: *TDB*
