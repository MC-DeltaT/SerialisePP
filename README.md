# Serialise++: Simple and efficient serialisation for C++

by Reece Jones

## Overview

Serialise++ is an easy-to-use library for efficient, generic serialisation of C++ values into bytes.
It uses a serialisation method inspired by [Cap'n Proto](https://capnproto.org/), to enable serialisation/deserialisation without copying large amount of data between C++ values and bytes buffers.
But unlike Cap'n Proto and similar libraries, Serialise++ enables definition of serialisable types in C++, with no extra code generation required!

## Features

 - Supported types:
   - Integers
   - Floating point numbers
   - Booleans
   - Static arrays
   - Dynamic arrays
   - Tuples
   - Optionals
   - Variants
   - User-defined structs
 - Extensible serialisation for custom types
 - On-the-fly serialisation and deserialisation - no data duplication!
 - Fully contained within C++ (no separate message/type definition, no code generation)
 - Complete `constexpr` support

(Support for more types is likely to come in the future.)

## Limitations

To achieve efficient serialisation/deserialisation while maintaining a simple serialisation format, Serialise++ is designed for fairly small messages.
Currently there is a hard limit of ~4GB of variable-sized data.
This limit may be expanded if a sufficient need for larger messages is found.

That said, Serialise++ is not designed for situations where literally every bit of serialised data or every CPU cycle spent serialising/deserialising is critical.
Some efficiency and performance must be forgone in exchange for generality, flexibility, and usability.

Serialise++ does not support "optional" fields like certain popular serialisation libraries do to enable backward-compatibility of message types.
This is a deliberate design decision to allow a simple serialisation format.

## Requirements

 - C++20
 - Platform with 8-bit bytes
 - \[For floating point support\] Platform with IEEE-754 floats
 - \[For CMake build\] CMake 3.22 or newer

## Dependencies

 - [SimpleTest](https://github.com/MC-DeltaT/SimpleTest) v1.0.0 or compatible

## Usage

Please see [ProgrammingGuide.md](ProgrammingGuide.md) for detailed usage information and code examples.

## Importing

There are a few methods of importing Serialise++ into your project, depending on your desired build process.  
Please see [Importing.md](Importing.md) for details.

## Serialisation Format

Please see [SerialisationFormat.md](SerialisationFormat.md) for details on how C++ values are represented as bytes.

## Tests

The library has unit tests made with [SimpleTest](https://github.com/MC-DeltaT/SimpleTest).
The test code is contained in `test/` and the CMake target is `SerialisePPTest`.
The target will only be built if the CMake variable `SERIALISEPP_BUILD_TEST` is set to `ON`.
