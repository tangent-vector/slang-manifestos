Versioning the Public Interfaces of Slang
=========================================

This document proposes an approach to the problem of *versioning* for the public interfaces that Slang exposes to users.

Background
----------

A user of Slang needs to think about source and binary compatibility of their code along multiple axes.
Code checked into the Slang project can affect compatiblity across muliple public interfaces, including:

* Changes to the Slang language, including its grammar and semantics.

* Changes to the *standard library*, including the types and functions it exposes.

* Changes to the Slang compilation API.

* Changes to the Slang compiler, including bug fixes, new features, etc.

Applications may care about the exact version(s) of these different interfaces for two main reasons.
First, they want to ensure that their code compiles against a version of the Slang project that provides *at least* the features/changes/fixes that their application relies on.
Second, they may want to ensure that their code doesn't use features/changes introduced *after* some particular version of an interface (e.g., to avoid having to increase the version of dependencies that a library package relies on).

In many cases, an application will not care if the versions of the different interfaces are in sync or not.
The application might want to be able to upgrade to the latest version of the compiler to pick up some bug fix, while still writing code against an earlier version of the Slang language.

Proposal
--------

We propose that the Slang project and codebase should use two version numbers, each corresponding to a subset of the public interfaces mentioned above:

* The *language version* indicates the capabilities of the Slang language *and* the standard library that accompanies it

* The *compiler version* indicates the capabilities of the Slang compilation API, the `slangc` executable, and the shared library that implements compilation

Importantly, a given version of the compiler should be able to support a *range* of versions of the language.

Language Versions
-----------------

Language versions should take the form `<major>.<minor>`, with the understanding that changes that only affect `<minor>` are intended to be purely additive.

Minor changes *may* break source compatibility for existing code (e.g., when a new keyword is introduced, but existing code was already using it as an ordinary identifier), but the team should avoid needless breaks in source compatiblity.
Minor changes *must not* break binary compatibility of existing Slang IR (once we get to the point of stabilizing the format of Slang IR binaries), and output binaries (e.g., DXIL libraries).

Major changes to the language may break source compatibility for existing code (e.g., by removing features entirely).
Major changes *should not* break binary compatibility except in cases where the team has determined that no other alternative is possible.

Compiler Versions
-----------------

Compiler versions should take the form `<major>.<minor>.<patch>`, with the understanding that changes that only affect `<patch>` are intended to be pure fixes, and changes that only affect `<minor>` and `<patch>` are intended to be purely additive.

We recommend that the the `<major>.<minor>` part of the language and compiler versions be related, in the following fashion:

* A compiler with version `X.Y.<patch>` should at the very least support language versions `X.0` through `X.Y`.

* A compiler with version `X.Y.<patch>` should not support language versions `Z.*` where `Z > X` nor `X.W` where `W > Y`.

* A compiler with version `X.Y.<patch>` *may* support language versions `Z.*` where `Z < X`. The exact range of versions supported should be documented for each compiler release.

Binary packages of the Slang project are numbered using the compiler version.

Choosing a Compiler Version
---------------------------

An application developer chooses the compiler version they want to build against by selecting the version number of the Slang release package they use.

Choosing a Language Version
---------------------------

Because of legacy conerns, the choice of language version to be used can come from various places.
We list these options from the highest priority (the ones the compiler will always use if available) to lowest.

### Version Directive in a Source File

A source unit being provided to the compiler may start with a *version directive* that specifies the language and language version that the file is written against.
The version directive may either be written in GLSL style, or Racket style.

A GLSL-style version directive for Slang looks like:

    #version <number> slang

where `<number>` is either:

* A sequence of digits forming an integer literal, in which case the corresponding language version is `X.YY` where `YY` is the last two digits, and `X` is all but the last two digits

* A sequence of digits with a single `.` in it, forming a floating-point literal `X.Y`, in which case the corresponding language version is `X.Y`. Note that in this case the `.` is required.

A Racket-style version directive for Slang looks like:

    #lang slang <version>

where `<version>` must be a sequence of digits with a single `.` in it, interpreted the same as in the GLSL-style case.

In either case:

* A file may only have one version directive, at most

* A version directive may only be preceeded by comments and whitespace

* A single module may include source files using different language versions, so long as all the versions are supported by a single compiler version

In files that the front-end detects as GLSL (or is told are GLSL), any language name other than `slang` should mean that the version number is interpreted as a GLSL version.
If `slang` is specified, then the file should be assumed to hold "Slang-flavored GLSL."

In files that the front-end detects as HLSL or Slang, the language name may be either `slang` or `hlsl`, which should set the compiler into "Slang mode" or "HLSL mode" accordingly.
When `hlsl` is the language name, the version number should be interpreted as naming a version of HLSL as defined and supported by dxc

Note that a future language version might make a version directive *required* in `.slang` source files.

### Version As Command-Line Argument

The user may pass a version as part of the `-lang` command-line option (and the equivalent API should be extended to support a version).
E.g., passing `-lang slang_X_Y` would select version `X.Y` of the Slang language.

A version specified via directive will always override a version specified via `-lang`.

### Implicit Version

If a language version is not specified by any other means, then a compiler with version `X.Y` should implicitly compile input Slang code as if it is using version `X.Y` of the Slang language.

Choosing A Compiler Version
---------------------------

For the most part, the compiler version to use is determined by the version of the `slang.dll` library and `slangc` binary that a developer uses.

However, clients of the Slang API should be able to set a preprocessor define for something like `SLANG_API_VERSION` which would cause the API header to `#if` out any API functions or types that were introduced *after* the given version. Similarly, the API version could be used to `#if` out API functions removed at or before that version, and to add deprecation attributes (for compilers that support them) on functions deprecated at or before that version.

The initialization function for a Slang "global session" should probably take an argument for `SLANG_API_VERSION`, so that the shared library can be informed of the compiler/API version that the client application expects.
At the very least, the library should error out if the application was compiled against a newer version of the API than what the library supports, since otherwise the application might attempt to do things like call methods on COM interfaces that were introduced in a later API verison (leading to a runtime crash).

Inside the Standard Library
---------------------------

Declarations inside the standard library will need to support a new attribute to indicate the version of the Slang language in which they first became available.
For example:

    [AvailableAt(9.3)]
    void someFunction(...);

In this case, `someFunction` is marked as only being available starting at language version 9.3.

Additional attributes can be used to mark declarations that have been deprecated or removed:

    [AvailableAt(9.3)]
    [DeprecatedAt(9.6)]
    [RemovedAt(10.0)]
    void someFunction(...);

Note that with sufficient design work, these attributes could be useful to Slang users as well, so that they could mark up declarations in their own library modules, based on a version number for the module (assuming that a future version of Slang supports associating a version number with a module).

Experimental Versions
---------------------

TODO: There needs to be a way to indicate the "bleeding edge" language or compiler version, where active development is happending.
This should be a language version where all features are enabled, including even experimental or incomplete features.

Inside the Compiler
-------------------

Each source unit that the compiler works with should track its *presumed* language and language version (based on `-lang` option or the implicit version).
During preprocessing, if a version directive if encountered, it can update the *actual* language and language version of the source unit.

During parsing, the version number of the source unit should be checked before parsing new constructs.
In the simplest case, where contextual keywords are used (e.g., for `where` clauses) a simple query could be introduced that checks both that the lookahead is what is expected *and* that the language version is high enough, e.g.: `advanceIf("where", LANGUAGE_VERSION_9_3)`.

During semantic checking, similar checks on language version should be introduced:

* Newly introduced constructs/features should be conditionalized on the language version

* When referencing standard library declarations, the `[AvailableAt]` and other attributes on the declaration should be checked to make sure it should be accessible to the language version of the source unit being compiled. Overload resolution can checking of language version as an additional filtering step, and place it appropriately in the sequence of disambiguation steps.

Internally, the compiler codebase should have something like a `SlangLanguageVersion` `enum` that lists all the language versions known as of that compiler version. Using a simple `enum` means that version checks can be one or two integer comparisons.

We reccomend that as individual features are being added to the compiler/language, an alias be added to the `SlangLanguageVersion` enumeration, for the new feature.
E.g., if a programmer is adding the new FizzBuzz feature, they might introduce:

    enum SlangLanguageVersion
    {
        // ...

        LANGUAGE_VERSION_FOR_FIZZBUZZ = LANGUAGE_VERSION_EXPERIMENTAL,
    }

New features would start their life attached to the "experimental" language version, which always represents the bleeding edge.
Any version checks against `LANGUAGE_VERSION_FOR_FIZZBUZZ` will automatically fail except for users opting into the experimental compiler version.
Once a feature is ready to be released to users and supported, the alias can be changed over to the proper language version number where support for the feature started.


Error Handling and Diagnostics
------------------------------

Ideally, the compiler should detect when code being compiled against one language version tries to use features only available in other versions (either later versions where the feature was added, or earlier versions before the feature was removed).

As discussed above, `[AvailableAt]` and similar attributes should only be filtered out of lookup results (for overload resolution, etc.) if there would still be one or more valid declarations seen after the filtering.
If the only declaration found by lookup (or the only candidate left during overload resolution) is one that is not available in the chosen language version, then a suitable error should be emitted.

During parsing, we may want to identify cases where the lookahead tokens unambiguously indicate a new/versioned language construct, and then parse that construct even if the chosen language version doesn't support it (in which case an error should be emitted).

Process
-------

The biggest impact of this proposal is on the development process.
In order to make language versions useful, we need to shape a development culture in which *every* new feature added to the language and/or compiler is both versioned and also comes with suitable version checks in the compiler implementation.
