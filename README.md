```
           _|                                  _|
   _|_|_|  _|  _|    _|    _|  _|          _|      _|_|_|      _|_|
 _|_|      _|_|      _|    _|  _|    _|    _|  _|  _|    _|  _|    _|
     _|_|  _|  _|    _|    _|  _|  _|  _|  _|  _|  _|    _|  _|    _|
 _|_|_|    _|    _|    _|_|_|    _|      _|    _|  _|    _|    _|_|_|
                           _|                                      _|
                       _|_|                                    _|_|
```

A high-reliability, real-time, decentralized platform for
collaborative autonomy. Tutorials for using Skywing can be found in
`documentation/tutorials`. To first build Skywing, follow the
instructions below.

# Building Skywing

Some dependencies are managed by Skywing's build process, and some you
need to acquire yourself beforehand.

## Dependencies Not Automatically Managed
 * compiler that supports c++17 library
   * tested: GCC/g++ (8.3.0) and LLVM/clang (6.0.0, 10.0.0, 11.0.0)
 * meson (https://mesonbuild.com/)
   * requires Python >= 3.8.0
 * CMake (https://cmake.org/)
 * ninja (https://ninja-build.org/)
 * Cap'n Proto (https://capnproto.org/)
   * requires version 0.8.0 or newer

## Dependencies Managed as Git Submodules

   You do not need to acquire these yourself.

 * Catch2
 * spdlog
 * Guidelines Support Library (GSL)

## Build instructions

   Follow these instructions to build a "barebones" version of Skywing without any tests or examples.

 * Build non-managed dependencies separately
 * Get dependencies
   * `git submodule update --init`
 * Create build files
   * `mkdir build`
   * `meson build`
 * Build Skywing
   * `cd build`
   * `ninja`
 * Run tests
   * `meson test --print-errorlogs -t 5.0`

## Enabling Tests and Examples

Before creating the build directory, replace the `meson build` command with

`meson build -Dbuild_tests=true -Dbuild_examples=true`

## Guidance for building on LC

If you are running on LLNL's LC clusters, these instructions can help you get set up.

### Building capnp
 * Cap'n Proto must be manually built first. Follow the instructions at https://capnproto.org/install.html#installation-unix, except you must build to a local directory. To do this, on the configure step, use
   * `./configure --prefix=capnp_build_dir`
   * This will create subdirectories `capnp_build_dir/bin`, `capnp_build_dir/include`, and `capnp_build_dir/lib`

### Building Skywing
 * Load meson (python), ninja, and switch to more recent version of gcc
   * 'ml python/3.8.2`
   * `ml ninja`
   * `ml gcc/9.3.1`
 * Add capnp pkgconfig directory to PKG_CONFIG_PATH
   * `export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:capnp_build_dir/lib/pkgconfig`
 * Follow build instructions as normal
 * To build the LC Hello World example, also include `-Dbuild_lc_examples=true` in the meson options.
 * To run the LC example, go to `(skywing_root)/build/examples/lc_hello_world/` and execute `source run.sh (bank_name)`. Note that you must have an active bank to run this test.

### Running Skywing
 * Add capnp shared library to LD_LIBRARY_PATH
   * `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:capnp_build_dir/lib`
 * Run as normal for running on login node. Note: can't run long jobs on login nodes!
 * Skywing configurations that involve many connections between agents can run into a file descriptor limit.  The soft limit can be increased by executing `ulimit -n <N>` where `<N>` must not exceed the hard limit (which is determined by executing `ulimit -Hn`) 

Note that Skywing configurations that involve many connections between agents can run into a file descriptor limit.
The soft limit can be increased by executing `ulimit -n <N>` where `<N>` must not exceed the hard limit (which can be determined by executing `ulimit -Hn`)

# Contributing to Skywing

Skywing is an open source project. We welcome contributions via pull
requests as well as questions, feature requests, or bug reports via
issues. Contact any of our team members with any questions. Please
also refer to our [code of conduct](CODE_OF_CONDUCT.md).

If you aren't a Skywing developer at LLNL, you won't have permission
to push new branches to the repository. First, you should create a
fork. This will create your copy of the Skywing repository and ensure
you can push your changes up to GitHub and create PRs.

* Create your branches off the `repo:main` branch.
* Clearly name your branches, commits, and PRs as this will help us manage queued work in a timely manner.
* Articulate your commit messages in the imperative (e.g., Adds new privacy policy link to README).
* Commit your work in logically organized commits, and group commits together logically in a PR.
* Title each PR clearly and give it an unambiguous description.
* Review existing issues before opening a new one. Your issue might already be under development or discussed by others. Feel free to add to any outstanding issue/bug.
* Be explicit when opening issues and reporting bugs. What behavior are you expecting? What is your justification or use case for the new feature/enhancement? How can the bug be recreated? What are any environment variables to consider?

# Development team
 * Aly Fox <fox33@llnl.gov>
 * Kendall Harter <harter8@llnl.gov>
 * **Colin Ponce <ponce11@llnl.gov>** (corresponding author)
 * Chris Vogl <vogl2@llnl.gov>

# License

Skywing is distributed here under under the GPL v2.0 license, but a
commercial license is also available. Users may choose either license,
depending on their needs.

For the commercial license, please inquire at <softwarelicensing@lists.llnl.gov>.

LLNL-CODE-835832