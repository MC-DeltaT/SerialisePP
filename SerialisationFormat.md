# Serialise++ Serialisation Format

## Overview

This document outlines the format which Serialise++ writes/reads bytes when serialising/deserialising.
Even though the library only has C++ support, the byte format should be stable enough to read and write in other languages, too.

All types are represented in terms of 8-bit bytes, which are the smallest addressable units of memory.
Serialise++ does not support platforms with bytes of different sizes.

For even more in-depth examples of bytes formats, please see the test code in `test/`.

## General Data Layout

Fundamental to Serialise++ are the concepts of the "fixed data" and "variable data" sections of a bytes buffer.

Fixed data contains data whose serialised size is known at compile time.
For example, when serialising and deserialising a `std::uint32_t`, you know the exact size of the value (4 bytes).

Variable data contains data whose size is not known until runtime.
For example, when serialising a `List<T>`, you don't know how many elements it has - and hence how much storage to allocate - until you look at the value at runtime.
Simiarly, when deserialising a `List<T>`, you don't know how many elements it has until you check the bytes buffer (the size will be in the fixed data).

Fixed data can be nested, to represent type composition.  
For a `SerialisableStruct` with fields of types `std::uint32_t` and `bool`, then the fixed data contains 4 bytes for the `std::uint32_t` and 1 byte for the `bool`.  
When serialising an `Optional<T>`, the fixed data of `T` will be contained within the segment of variable data associated with the `Optional<T>`, because it is not known if the `T` is present until runtime.  
Variable data is not nested, to allow efficiently indexing. There is only 1 global variable data section, which is always at the end of the buffer. When serialising, it is appended to as required.

The layout of a complete byte buffer might look something like this:
<pre>
Global fixed data section
  Root object fixed data
    Object A fixed data
      Object A.A fixed data
      Object A.B fixed data
    Object B fixed data
Global variable data section
  Object A variable data
    Object A.C fixed data
  Object A.C variable data
  Object B variable data
    Object B.A fixed data
      Object B.A.A fixed data
</pre>
Here objects A, A.A, A.B, and B were known to exist at compile time, thus are in the fixed data section. Objects A.C and B.A were not known to exist until runtime, and thus are in the variable data section.  
Notice how object B.A is variable data, thus its subobject B.A.A is in the variable data section too. Also notice how object A.C's variable data is not nested within it.

## Representations of Types

In general, there is no padding inserted between data. There is no concept of alignment within the bytes buffer.

### `std::byte`

Fixed data: 1 byte, the value is represented as-is.

Variable data: none.

### Integers

Includes the standard C++ integral types, including `char`s, except `bool`.

Fixed data: the integer is represented in two's complement, with little-endian byte order. The number of bytes of an integer type `I` is exactly `sizeof(I)` (since C++20 requires two's complement).

Variable data: none.

Note that the values of character literals encoded in `char` types will depend on the character set used by the compiler (which I believe is implementation-defined until C++23).

Example:  
`std::int32_t` with the value -1'234'567.  
<pre>
offset: value
0x00  : 0x79    # least significant byte
0x01  : 0x29
0x02  : 0xED
0x03  : 0xFF    # most significant byte
</pre>

### `bool`

Fixed data: 1 byte. Serialise++ serialises `true` to `0x01` and `false` to `0x00` specifically, but when deserialising, any nonzero value is accepted as `true`.

Variable data: none.

### Floating point numbers

Includes `float` and `double`.

Fixed data: the value is represented in IEEE-754 binary format, with little endianness. `float` (IEEE-754 binary32) is 4 bytes, `double` (IEEE-754 binary64) is 8 bytes.

Variable data: none.

Example:  
`float` with the value 123456.
<pre>
offset: value
0x00  : 0x00
0x01  : 0x20
0x02  : 0xF1
0x03  : 0x47
</pre>

### `Array<T, N>`

Fixed data: just the fixed data for all the elements, in the same order as in the `SerialiseSource`.

Variable data: just the variable data for the elements, in the same order as in the `SerialiseSource`.

Example:  
`Array<std::uint16_t, 4>` containing the elements `{12, 45, 465, 24643}`.
<pre>
offset: value
0x00  : 0x0C    # element 0
0x01  : 0x00
0x02  : 0x2D    # element 1
0x03  : 0x00
0x04  : 0xD1    # element 2
0x05  : 0x01
0x06  : 0x43    # element 3
0x07  : 0x60
</pre>

### `Optional<T>`

Fixed data: a `std::uint16_t` specifying the "value offset" (see below). 0 indicates no value is contained.

Variable data: if a value is contained, then the representation of that value of type `T`. If no value is contained, then no variable data is used.

If a value is contained, the fixed data section of the contained `T` will begin at byte \[`value offset - 1`\] of the variable data section. The contained `T`'s variable data section will follow after.

Example 1:  
`Optional<std::uint32>` containing the value 123'456'789.  
Assume the variable data section already has 600 bytes of other data.
<pre>
offset: value
0x0000: 0x59    # value offset (600 + 1)
0x0001: 0x02
...             # other variable data
0x025A: 0x15    # contained uint32_t value
0x025B: 0xCD
0x025C: 0x5B
0x025D: 0x07
</pre>

Example 2:  
`Optional<Optional<std::int8_t>>` containing the value -123.  
Assume the variable data section already has 5 bytes of other data.
<pre>
offset: value
0x00  : 0x06    # outer optional value offset (5 + 1)
0x01  : 0x00
...             # other variable data
0x07  : 0x08    # inner optional value offset (7 + 1)
0x08  : 0x00
0x09  : 0x85    # inner optional contained int8_t value
</pre>

### `List<T>`

Fixed data:

 1. A `std::uint16_t` specifying the number of contained elements.
 2. A `std::uint16_t` specifying the offset of the first element from the start of the variable data section.

Variable data: the representations of the contained elements, in the same order as in the `SerialiseSource`. All the elements' fixed data occurs in the variable data section *before* the elements' variable data.

If the element count is 0, then the value of the offset is not significant.

Example 1:  
`List<std::int8_t>` containing the elements `{1, 2, 3, 4, 5}`.  
Assume the variable data section already has 1000 bytes of other data.
<pre>
offset: value
0x0000: 0x05    # element count
0x0001: 0x00
0x0002: 0xE8    # element offset
0x0003: 0x03
...             # other variable data
0x03EC: 0x01    # element 0
0x03ED: 0x02    # element 1
0x03EE: 0x03    # element 2
0x03EF: 0x04    # element 3
0x03F0: 0x05    # element 4
</pre>

Example 2:  
`List<Optional<std::uint8_t>>` containing the elements `{1, null, 3, 4, null}`.  
Assume the variable data section already has 100 bytes of other data.
<pre>
offset: value
0x00  : 0x05    # element count
0x01  : 0x00
0x02  : 0x64    # element offset
0x03  : 0x00
...             # other variable data
0x68  : 0x6F    # element 0 value offset
0x69  : 0x00
0x6A  : 0x00    # element 1 value offset
0x6B  : 0x00
0x6C  : 0x70    # element 2 value offset
0x6D  : 0x00
0x6E  : 0x71    # element 3 value offset
0x6F  : 0x00
0x70  : 0x00    # element 4 value offset
0x71  : 0x00
0x72  : 0x01    # element 0 value
0x73  : 0x03    # element 2 value
0x74  : 0x04    # element 3 value
</pre>

### `SerialisableStruct<Fs...>`

Fixed data: just the fixed data for all fields `Fs...`, in the same order as in the template argument list.

Variable data: just the variable data for all fields `Fs...`, in the same order as in the template argument list.

Example:  
```c++
SerialisableStruct<
    Field<"a", std::uint16_t>,
    Field<"b", Optional<std::uint32_t>>,
    Field<"c", std::uint8_t>>,
    Field<"d", Optional<std::uint8_t>>>
```
With the value `{a=1234, b=567890, c=10, d=20}`.  
Assume the variable data section already has 20 bytes of other data.
<pre>
offset: value
0x00  : 0xD2    # a
0x01  : 0x04
0x02  : 0x15    # b value offset
0x03  : 0x00
0x04  : 0x0A    # c
0x05  : 0x19    # d value offset
0x06  : 0x00
...             # other variable data
0x1B  : 0x52    # b value
0x1C  : 0xAA
0x1D  : 0x08
0x1E  : 0x00
0x1F  : 0x14    # d value
</pre>
