# Serialise++ Serialisation Format

## Overview

This document outlines the format which Serialise++ writes/reads bytes when serialising/deserialising. Even though the library only has C++ support, the byte format should be stable enough to read and write in other languages, too.

All types are represented in terms of 8-bit bytes, which are the smallest addressable units of memory. Serialise++ does not support platforms with bytes of different sizes.

For even more in-depth examples of bytes formats, please see the test code in `test/`.

## General Data Layout

Fundamental to Serialise++ are the concepts of the "fixed data" and "variable data" sections of a bytes buffer.

Fixed data contains data whose serialised size is known at compile time. For example, when serialising and deserialising a `std::uint32_t`, you know the exact size of the value (4 bytes).

Variable data contains data whose size is not known until runtime. For example, when serialising a `dynamic_array<T>`, you don't know how many elements it has - and hence how much storage to allocate - until you look at the value at runtime. Simiarly, when deserialising a `dynamic_array<T>`, you don't know how many elements it has until you check the bytes buffer (the size will be in the fixed data).

Fixed data can be nested, to represent type composition.  
For a `record` with fields of types `std::uint32_t` and `bool`, then the fixed data contains 4 bytes for the `std::uint32_t` and 1 byte for the `bool`.  
When serialising an `optional<T>`, the fixed data of `T` will be contained within the segment of variable data associated with the `optional<T>`, because it is not known if the `T` is present until runtime.  
Variable data is not nested, to allow efficiently indexing. There is only one global variable data section, which is always at the end of the buffer. When serialising, it is appended to as required.

The layout of a complete byte buffer might look something like this:

    Buffer
      Fixed data section
        Root object fixed data
          Object A fixed data
            Object A.A fixed data
            Object A.B fixed data
          Object B fixed data
      Variable data section
        Object A variable data
          Object A.C fixed data
        Object A.C variable data
        Object B variable data
          Object B.A fixed data
            Object B.A.A fixed data

Here objects A, A.A, A.B, and B were known to exist at compile time, thus are in the fixed data section. Objects A.C and B.A were not known to exist until runtime, and thus are in the variable data section.  
Notice how object B.A is variable data, thus its subobject B.A.A is in the variable data section too. Also notice how object A.C's variable data is not nested within it.

## Representations of Types

In general, there is no padding inserted between data. There is no concept of alignment within the bytes buffer.

### `null`

Fixed data: none.

Variable data: none.

### `std::byte`

Fixed data: 1 byte, the value is represented as-is.

Variable data: none.

### Integers

Includes the standard C++ integral types, except `bool`.

Fixed data: the integer is represented in two's complement, with little-endian byte order. The number of bytes of an integer type `I` is exactly `sizeof(I)` (since C++20 requires two's complement).

Variable data: none.

Note that the values of character literals encoded in `char` types will depend on the character set used by the compiler, which is largely implementation-defined.
As a result, it is your responsibility to ensure consistent encodings when serialising and deserialising character data with Serialise++.

Example:  
`std::int32_t` with the value -1'234'567.  

    offset: value
    0x00  : 0x79    # least significant byte
    0x01  : 0x29
    0x02  : 0xED
    0x03  : 0xFF    # most significant byte

### `bool`

Fixed data: 1 byte. Serialise++ serialises `true` to `0x01` and `false` to `0x00` specifically, but when deserialising, any nonzero value is accepted as `true`.

Variable data: none.

### Floating Point Numbers

Includes `float` and `double`.

Fixed data: the value is represented in IEEE-754 binary format, with little endianness. `float` is IEEE-754 binary32 (4 bytes), `double` is IEEE-754 binary64 (8 bytes).

Variable data: none.

Example:  
`float` with the value 123456.

    offset: value
    0x00  : 0x00    # least significant byte
    0x01  : 0x20
    0x02  : 0xF1
    0x03  : 0x47    # most significant byte

### `optional<T>`

Fixed data: a `std::uint32_t` specifying the "value offset" (see below). 0 indicates no value is contained.

Variable data: if a value is contained, then the representation of that value of type `T`. If no value is contained, then no variable data is used.

If a value is contained, the fixed data of the contained `T` will begin at byte \[`value offset - 1`\] of the buffer. The variable data of the contained `T` (if any) will follow after.

Example 1:  
`optional<std::uint32>` containing the value 123'456'789.  
Assume the buffer had a size of 600 at the point of serialisation.

    offset: value
    0x0000: 0x59    # value offset (600 + 1)
    0x0001: 0x02
    0x0002: 0x00
    0x0003: 0x00
    ...             # other data
    0x0258: 0x15    # contained uint32_t value
    0x0259: 0xCD
    0x0260: 0x5B
    0x0261: 0x07

Example 2:  
`optional<optional<std::int8_t>>` containing the value -123.  
Assume the buffer had a size of 10 at the point of serialisation.

    offset: value
    0x00  : 0x0B    # outer optional value offset (10 + 1)
    0x01  : 0x00
    0x02  : 0x00
    0x03  : 0x00
    ...             # other data
    0x0A  : 0x0F    # inner optional value offset (14 + 1)
    0x0B  : 0x00
    0x0C  : 0x00
    0x0D  : 0x00
    0x0E  : 0x85    # inner optional contained int8_t value

### `variant<Ts...>`

Fixed data:

 1. A `std::uint8_t` specifying the zero-based index of the contained type.
 2. A `std::uint32_t` specifying the offset of the contained value from the start of the buffer.

Variable data: the representation of the contained value (could be any of `Ts`).

Example 1:  
`variant<std::int64_t, optional<std::uint32_t>, float>` containing the type `optional<std::uint32_t>` and the value 8192.  
Assume the buffer had a size of 20 at the point of serialisation.

    offset: value
    0x00  : 0x01    # variant type index
    0x01  : 0x14    # variant value offset
    0x02  : 0x00
    0x03  : 0x00
    0x04  : 0x00
    ...             # other data
    0x14  : 0x19    # optional value offset
    0x15  : 0x00
    0x16  : 0x00
    0x17  : 0x00
    0x18  : 0x00    # optional value
    0x19  : 0x20
    0x1A  : 0x00
    0x1B  : 0x00

### `dynamic_array<T>`

Fixed data:

 1. A `std::uint32_t` specifying the number of contained elements.
 2. A `std::uint32_t` specifying the offset of the first element from the start of the buffer.

Variable data: the representations of the contained elements, in the same order as in the `serialise_source`. All the elements' fixed data occurs in the buffer *before* the elements' variable data.

If the element count is zero, then the value of the offset is not significant.

Example 1:  
`dynamic_array<std::int8_t>` containing the elements `{1, 2, 3, 4, 5}`.  
Assume the buffer had a size of 1000 at the point of serialisation.

    offset: value
    0x0000: 0x05    # element count
    0x0001: 0x00
    0x0002: 0x00
    0x0003: 0x00
    0x0004: 0xE8    # element offset
    0x0005: 0x03
    0x0006: 0x00
    0x0007: 0x00
    ...             # other data
    0x03E8: 0x01    # element 0
    0x03E9: 0x02    # element 1
    0x03EA: 0x03    # element 2
    0x03EB: 0x04    # element 3
    0x03EC: 0x05    # element 4

Example 2:  
`dynamic_array<optional<std::uint8_t>>` containing the elements `{1, nullopt, 3, nullopt}`.  
Assume the buffer had a size of 100 at the point of serialisation.

    offset: value
    0x00  : 0x04    # element count
    0x01  : 0x00
    0x02  : 0x00
    0x03  : 0x00
    0x04  : 0x64    # element offset
    0x05  : 0x00
    0x06  : 0x00
    0x07  : 0x00
    ...             # other data
    0x64  : 0x75    # element 0 value offset
    0x65  : 0x00
    0x66  : 0x00
    0x67  : 0x00
    0x68  : 0x00    # element 1 value offset
    0x69  : 0x00
    0x6A  : 0x00
    0x6B  : 0x00
    0x6C  : 0x76    # element 2 value offset
    0x6D  : 0x00
    0x6E  : 0x00
    0x6F  : 0x00
    0x70  : 0x00    # element 3 value offset
    0x71  : 0x00
    0x72  : 0x00
    0x73  : 0x00
    0x74  : 0x01    # element 0 value
    0x75  : 0x03    # element 2 value

### `static_array<T, Size>`

Fixed data: just the fixed data for all the elements, in the same order as in the `serialise_source`.

Variable data: just the variable data for the elements, in the same order as in the `serialise_source`.

Example:  
`static_array<optional<std::uint16_t>, 4>` containing the elements `{12, nullopt, 465, 24643}`.  
Assume the buffer had a size of 200 at the point of serialisation.

    offset: value
    0x00  : 0xC9    # element 0 value offset
    0x01  : 0x00
    0x02  : 0x00
    0x03  : 0x00
    0x04  : 0x00    # element 1 value offset
    0x05  : 0x00
    0x06  : 0x00
    0x07  : 0x00
    0x08  : 0xCB    # element 2 value offset
    0x09  : 0x00
    0x0A  : 0x00
    0x0B  : 0x00
    0x0C  : 0xCD    # element 3 value offset
    0x0D  : 0x00
    0x0E  : 0x00
    0x0F  : 0x00
    ...             # other data
    0xC8  : 0x0C    # element 0 value
    0xC9  : 0x00
    0xCA  : 0xD1    # element 2 value
    0xCB  : 0x01
    0xCC  : 0x43    # element 3 value
    0xCD  : 0x60

### `pair<T1, T2>`

Fixed data: just the fixed data for `T1`, then `T2`.

Variable data: just the variable data for `T1`, then `T2`.

Example:  
`pair<optional<std::uint32_t>, std::int16_t>` containing the values 1234567 and -12345.  
Assume the buffer had a size of 30 at the point of serialisation.

    offset: value
    0x00  : 0x1F    # optional value offset
    0x01  : 0x00
    0x02  : 0x00
    0x03  : 0x00
    0x04  : 0xC7    # int16_t
    0x05  : 0xCF
    ...             # other data
    0x1E  : 0x87    # optional value
    0x1F  : 0xD6
    0x20  : 0x12
    0x21  : 0x00

### `tuple<Ts...>`

Fixed data: just the fixed data for all elements `Ts...`, in the same order as in the template argument list.

Variable data: just the variable data for all elements `Ts...`, in the same order as in the template argument list.

Example:  
`tuple<std::uint8_t, optional<std::uint32_t>, std::int16_t>` containing the values `{123, 456789, 87}`.  
Assume the buffer had a size of 55 at the point of serialisation.

    offset: value
    0x00  : 0x7B    # uint8_t
    0x01  : 0x38    # optional value offset
    0x02  : 0x00
    0x03  : 0x00
    0x04  : 0x00
    0x05  : 0x57    # int16_t
    0x06  : 0x00
    ...             # other data
    0x37  : 0x55    # optional value
    0x38  : 0xF8
    0x39  : 0x06
    0x3A  : 0x00

### `record`

Fixed data: the fixed data for the base `record` (if any), followed by the fixed data for fields in the derived `record` in the same order as in the template argument list.

Variable data: the variable data for the base `record` (if any), followed by the variable data for fields in the derived `record` in the same order as in the template argument list.

Example:  

```c++
using base_record = record<
    field<"a", std::uint16_t>
>;

record<
    base<base_record>,
    field<"b", optional<std::uint32_t>>,
    field<"c", std::uint8_t>>,
    field<"d", optional<std::uint8_t>>>
```

With the value `{a=1234, b=567890, c=10, d=20}`.  
Assume the buffer had a size of 20 at the point of serialisation.

    offset: value
    0x00  : 0xD2    # a
    0x01  : 0x04
    0x02  : 0x15    # b value offset
    0x03  : 0x00
    0x04  : 0x00
    0x05  : 0x00
    0x06  : 0x0A    # c
    0x07  : 0x19    # d value offset
    0x08  : 0x00
    0x09  : 0x00
    0x0A  : 0x00
    ...             # other data
    0x14  : 0x52    # b value
    0x15  : 0xAA
    0x16  : 0x08
    0x17  : 0x00
    0x18  : 0x14    # d value
