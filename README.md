[ubuntu-x86]: https://github.com/FedericoBruzzone/ouroboros/actions/workflows/ubuntu-x86.yml
[ubuntu-x86-shield]: https://github.com/FedericoBruzzone/ouroboros/actions/workflows/ubuntu-x86.yml/badge.svg
[apple-silicon]: https://github.com/FedericoBruzzone/ouroboros/actions/workflows/apple-silicon.yml
[apple-silicon-shield]: https://github.com/FedericoBruzzone/ouroboros/actions/workflows/apple-silicon.yml/badge.svg
[apple-x86]: https://github.com/FedericoBruzzone/ouroboros/actions/workflows/apple-x86.yml
[apple-x86-shield]: https://github.com/FedericoBruzzone/ouroboros/actions/workflows/apple-x86.yml/badge.svg

<p><div align="center" style="text-align: center">
<img src="static/ouroboros.jpg"
     alt="Ouroboros"
     title="Ouroboros"
     width="250"
     style=""/>
<br/>
<figcaption>
Theodoros Pelecanos, 1478, <i>Miniature depicting an ouroboros</i>, 
from the copy of a lost alchemical treatise attributed to Synesius of Cyrene (370–413)
<a href="https://en.wikipedia.org/wiki/Ouroboros">Source</a>
</figcaption>
</div></p>

# Ouroboros [/ˌʊərəˈbɒrəs/]

[![Ubuntu x86][ubuntu-x86-shield]][ubuntu-x86]
[![Apple Silicon][apple-silicon-shield]][apple-silicon]
[![Apple x86][apple-x86-shield]][apple-x86]

> [!WARNING]
> The pass is not yet implemented and is currently under active development.

> [!NOTE] 
> This repository is a fork of [llvm-pass-template](https://github.com/FedericoBruzzone/llvm-pass-template)

A specialized LLVM pass designed to reimagine **Tail Recursion Elimination** (TRE) through an out-of-tree implementation.
Guy L. Steele Jr. in 1977 ([DOI](https://dl.acm.org/doi/10.1145/800179.810196)) observed that certain kinds of function calls, specifically tail calls, can be optimized to avoid increasing the call stack depth — a technique now known as Tail Call Optimization (TCO).
TRE is a specific form of TCO that focuses on optimizing tail-recursive functions, which are functions that call themselves as their final action.

## Motivation: Why Redesign TRE?

Consider this fundamental example that illustrates the limitations of current TRE implementations:

```c
int f(int x) {
    if (x == 1) return x;
    return 2 * f(x-1);
}
```

While GCC successfully optimizes this function, LLVM's current TRE pass fails to eliminate the recursion even at the highest optimization level (-O3), as demonstrated in [Compiler Explorer](https://godbolt.org/z/cP5oYe7K4). This optimization gap results in significant performance penalties, potential stack overflow vulnerabilities for recursive algorithms, and last but not least, the inability to apply other optimizations (e.g., loop optimizations).

Strictly speaking, this function violates the traditional definition of [tail recursion](https://en.wikipedia.org/wiki/Tail_call) due to the multiplication operation occurring after the recursive call. However, leveraging the mathematical properties of commutativity and associativity inherent in multiplication allows us to **see** and **reason** about the function in a different light. While we do not perform this specific transformation in our implementation, recognizing that such a transformation is theoretically possible enables more sophisticated analysis:

```c
int f(int x, int acc = 1) {
    if (x == 1) return acc;
    return f(x-1, 2 * acc);
}
```

The scope of missed optimization opportunities extends far beyond this simple case. Consider these additional scenarios where existing TRE passes fall short:

- **[Multiple Base Cases](https://godbolt.org/z/MzaTMKd85)**: Functions with complex termination conditions
- **[Dynamic Stack Growth](https://godbolt.org/z/K15W5nh6n)**: Variable-length arrays (VLAs) where even GCC struggles
- **Multiple Recursive Calls**: Functions like [Fibonacci](https://godbolt.org/z/1bx8vEbxj) that require advanced transformation techniques
- **Non-Primitive Recursive Functions**: Complex algorithms such as the [Ackermann function](https://godbolt.org/z/odPGYsbdc)

The challenge of eliminating multiple recursion patterns represents a significantly more complex optimization problem than single tail-call elimination, demanding sophisticated analysis and transformation techniques that we will explore in future iterations of this work.

## Features

This repository is shipped with:
- The LLVM pass plugin that can be loaded into the `opt` tool (see [Run the pass with `opt`](#run-the-pass-with-opt) section for more details).
- The LLVM pass that can be registered as part of an existing LLVM (both `clang` and `opt`) default pipeline (see [Run the pass against an existing default LLVM pipeline](#run-the-pass-against-an-existing-default-llvm-pipeline) section for more details).
- A standalone tool (`tre`) to run the **TailRecursionElimination** pass without relying on `opt` (see [Run the pass as executable](#run-the-pass-as-executable) section for more details).
- A set of unit and regression tests for the **TailRecursionElimination** pass using `llvm-lit` and `FileCheck` (see [Testing](#testing) section for more details).
- A benchmarking setup using `llvm-test-suite` to measure the performance impact of the **TailRecursionElimination** pass (see [Benchmarking against llvm-test-suite](#benchmarking-against-llvm-test-suite) section for more details).

## Building the pass

The [GUIDELINES.md](GUIDELINES.md) file includes detailed instructions on how to set up the system to have an LLVM 21 installation, and how to set the `LLVM_DIR` environment variable accordingly.
For instance:
```bash
export LLVM_DIR=/usr/local/opt/llvm@21 # macOS Intel via Homebrew
export LLVM_DIR=/opt/homebrew/opt/llvm@21 # macOS Apple Silicon via Homebrew
export LLVM_DIR=/usr/lib/llvm-21 # Ubuntu x86_64 via apt
export LLVM_DIR=~/llvm-project/llvm-build # LLVM 21 built
```

To easily emit _human-readable_ LLVM IR files (`.ll`) from C/C++ source files, you can use `clang`:

```bash
${LLVM_DIR}/bin/clang -fno-discard-value-names -S -emit-llvm <file>.c -o <file>.ll
```

To build the pass, run the following commands from the root of the repository:

```bash
mkdir build
cd build
cmake -DLLVM_INSTALL_DIR=${LLVM_DIR} .. # -G Ninja
make # ninja
```

> Note that, the `build` folder is hardcoded within `.clangd` file to provide better IDE support. `clangd` uses the `compile_commands.json` file generated by CMake to provide accurate code completion and navigation features. If you decide to use a different build folder, make sure to update the path in `.clangd` accordingly.

## Run the pass with `opt`

By assuming you have built the pass successfully, you can run it using the `opt` tool provided by your LLVM installation.
To run the pass on an LLVM IR file, use the `opt` tool as follows:

```bash
${LLVM_DIR}/bin/opt \
    --load-pass-plugin build/lib/libTailRecursionElimination.{dylib|so} \
    --passes="print<tailrecelim>" \
    -disable-output \
    <file>.ll
```

> Note that, `-disable-output` is used to avoid generating an output file. If you want to generate an output file, replace `-disable-output` with `-S -o <output-file>.ll`. Assuming you have an LLVM assert build, if your pass leverages the LLVM internal logger, you can either enable logging globally with `-debug` or selectively for your pass only. To enable logging for your pass only, after the definition of the `DEBUG_TYPE=<what-you-want>` macro, you can enable logging by passing the `-debug-only="<what-you-want>"` flag to `opt`.

## Run the pass against an existing default LLVM pipeline

> [!NOTE]
> This section assume that your pass is registered as a step of an existing LLVM pipeline. 
> To do so, you need to register your pass within the `getTailRecursionEliminationPluginInfo()` function by using, for instance, `PB.registerPipelineStartEPCallback` which registers your pass at the start of a default pipeline.
> Of course, there are a plethora of other registration methods you can use depending on where you want to insert your pass within the pipeline.
> The [TailRecursionElimination.cpp](lib/TailRecursionElimination.cpp) file registers the **TailRecursionElimination** pass by using `PB.registerVectorizerStartEPCallback` as an example.

By assuming you have built the pass successfully, you can leverage an existing LLVM (both `clang` and `opt`) default (`-O{1,2,3,s,z}`) pipeline to run the pass as a plugin during the lowerings/optimizations.

```bash
${LLVM_DIR}/bin/clang \
    -fpass-plugin=build/lib/libTailRecursionElimination.{dylib|so} \
    -fno-discard-value-names \
    -O{1,2,3,s,z} \ # Pick one optimization level
    -S -emit-llvm \
    <file>.c -o <file>.ll
```

Optionally, you can also specify additional flags:

- `-Rpass=<pass-name>`: to report optimizations performed by the pass.
- `-Rpass-missed=<pass-name>`: to report missed optimizations by the pass.
- `-Rpass-analysis=<pass-name>`: to report analyses performed by the pass.

Or with `opt`:

```bash
${LLVM_DIR}/bin/opt \
    --load-pass-plugin build/lib/libTailRecursionElimination.{dylib|so} \
    --passes="default<O{1,2,3,s,z}>" \ # Pick one optimization level
    -Rpass=tailrecelim \
    -disable-output \
    <file>.ll
```

## Run the pass as executable

You can run the analysis pass through a standalone tool called `tre`.
`tre` is an LLVM based executable defined in
[TailRecursionEliminationMain.cpp](https://github.com/FedericoBruzzone/ouroboros/blob/main/bin/TailRecursionEliminationMain.cpp).
It is a command line wrapper that allows you to run **TailRecursionElimination** without the need for **opt**:

```bash
<build_dir>/bin/tre <file>.ll
```

It is an example of a relatively basic static analysis tool. Its implementation demonstrates how basic pass management in LLVM works (i.e. it handles that for itself instead of relying on **opt**).

## Testing

In order to run the tests, you need to install `llvm-lit` (i.e., `lit`).
If you have LLVM built from sources, `llvm-lit` is located in the `<llvm-build-dir>/bin/` folder.
On the other hand, it is not bundled with LLVM 21 packages, but you can install it with `pip` or `brew` (on macOS).

```bash
{pip | brew} install lit
```

To run the tests, execute the following command from the root of the repository:

```bash
lit -v -a ./build/test
```


## Benchmarking against llvm-test-suite

The `benchmarking` directory contains scripts and documentation (its [README.md](benchmarking/README.md) file) for benchmarking the `TailRecursionElimination` LLVM pass using the `llvm-test-suite`.


## License

This repository is licensed under either of

* [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0) with LLVM-exception
* [MIT License](http://opensource.org/licenses/MIT)

at your option.

Please review the license file provided in the repository for more information regarding the terms and conditions of the license.

## Contact

If you have any questions, suggestions, or feedback, do not hesitate to [contact me](https://federicobruzzone.github.io/).
