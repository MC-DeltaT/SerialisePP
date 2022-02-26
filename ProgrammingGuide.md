# Serialise++ Programming Guide

## Setup

Include `serialpp/serialpp.hpp` to pull in all functionality.

All Serialise++ code entities live in the `serialpp` namespace. The `serialpp::` namespace specifier is omitted in this guide for brevity.

## Serialisation Basics

The serialisation process is, conceptually, a function which maps C++ values (input) to raw bytes (output).  
In Serialise++, the code entities associated with this process are:

 - `SerialiseSource<T>` - contains the C++ values.
 - `serialise<T>()` - performs the serialisation.
 - `SerialiseBuffer` - storage for the raw bytes.

Here, `T` is the type we are interested in serialising.

Let's take a look at an example of serialising a `std::uint64_t`:

```c++
// Set up what value we want to serialise.
SerialiseSource<std::uint64_t> const source{42};
// Declare storage for the serialised bytes.
SerialiseBuffer buffer;
// Perform the serialisation.
serialise(source, buffer);
// Get a view of the resulting bytes. ConstBytesView is just an alias to std::span<std::byte const>
ConstBytesView bytes_view = buffer.span();
```

That's it! `buffer`'s contents now contain the bytes representation of a `std::uint64_t` with value 42.
We obtain a view of those bytes by using `buffer.span()`.

Even with more complex types, the process is the same.
`SerialiseSource<T>` holds the value to be serialised, `serialise<T>()` does the serialisation, and `SerialiseBuffer` holds the result.

One important thing to note is that `SerialiseSource<T>` need not *store* the value to be serialised.
It may instead store some form of generator for the value (e.g. a lazily-evaluated range, for the `List` type).
This enables serialisation without excessive copying of data.

(You may be wondering why `SerialiseBuffer` exists - why not just return, say, a `std::vector<std::byte>` from `serialise()`? The reason is performance. Memory allocations are slow and we'd like to avoid them.)

## Deserialisation Basics

The deserialisation process is similar to the serialisation process, but in reverse.
The Serialise++ code entities associated with it are:

 - `Deserialiser<T>` - obtains C++ values from a buffer of bytes.
 - `deserialise<T>()` - builds a `Deserialiser<T>` from a view of bytes.

Again, `T` is the type we want to deserialise (note that you must know this in advance, Serialise++ does not provide any built-in way to query what type is held by a bytes buffer).

Example code for deserialising a `std::uint64_t`:

```c++
// The buffer with the bytes representation.
ConstBytesView bytes = ...;
// Obtain a deserialiser object.
Deserialiser<std::uint64_t> const deserialiser = deserialise<std::uint64_t>(bytes);
// Perform the deserialisation.
std::uint64_t const value = deserialiser.value();
```

It's important to note that the deserialisation didn't occur until the call to `deserialiser.value()`.
This on-the-fly deserialisation avoids copying the entire value into a large C++ object, and then copying again what you need from that object.

Also note that the behaviour of `Deserialiser<T>` is dependent on `T`.
Different types have different member functions for deserialisation.
Particularly, `Deserialiser` for compound types can produce more `Deserialiser` instances for the contained data.

## Type Support

Serialise++ provides types to support more complex data.
Each type is specialised for `SerialiseSource`, `Serialiser`, and `Deserialiser` to provide the required functionality.

### Scalars

Scalars in Serialise++ are the basic C++ types. These currently include:

 - Unsigned integers
 - Signed integers
 - `bool`
 - `std::byte`
 - `float` and `double` (supported only if they are IEEE-754 binary32 and binary64, respectively, and aren't mixed endian)

`SerialiseSource` for a scalar is a transparent wrapper around the scalar itself, stored in the data member `value`.
It's constructible from a scalar value and implicitly convertible to the scalar value.

`Deserialiser` for a scalar provides a member function `value()` which deserialises and returns the scalar value.

Because scalars the bread-and-butter types, they support "automatic deserialisation" in many contexts within Serialise++.
If a nonscalar type `C` contains a scalar type `S`, then `Deserialiser<C>` won't give you a `Deserialiser<S>`, it will just give you the instance of `S` already deserialised.

### Array

`Array<T, N>` is a type which contains an ordered sequence of exactly `N` elements of type `T`.

`SerialiseSource` for an `Array<T, N>` contains a data member `elements` of type `SerialiseSource<T>[N]` if `N > 0`, or nothing if `N == 0`.
It can be initialised like an `std::array`:
```c++
SerialiseSource<Array<long, 4>> const source{{1, 2, 3, 4}};
```

`Deserialiser` for an `Array<T, N>` has the following member functions:

 - `size()`: returns the number of elements (always `N`).
 - `operator[]`: returns a `Deserialiser<T>` (or `T` for scalar `T`) for an element at the specified index. The index must be in the range `[0, N)`.
 - `at()`: like `operator[]` but throws `std::out_of_range` if the index is out of bounds.
 - `get<I>()`: like `operator[]`, but checks the index at compile time.
 - `elements()`: returns a view of that yields `Deserialiser<T>` (or `T` for scalar `T`) for each element.

### List

`List<T>` is a type which contains a variable-size ordered sequence of `T`.

`SerialiseSource` for a `List<T>` wraps a C++20 range whose elements are convertible to `SerialiseSource<T>`.
It may be constructed as follows:

```c++
// Default construct to contain 0 elements:
SerialiseSource<List<long>> const source;

// Construct from a braced initialiser of SerialiseSource<T>:
SerialiseSource<List<long>> const source{{1, 2, 3}};

// Construct to view (not own) a range:
std::vector<int> vec{1, 2, 3};
SerialiseSource<List<long>> const source{vec};

// Construct to own (by moving) a range:
std::vector<int> vec{1, 2, 3};
SerialiseSource<List<long>> const source{std::move(vec)};

// Construct to own a viewable_range:
auto const v = std::ranges::views::iota(1, 4);
SerialiseSource<List<long>> const source{v};
```

`Deserialiser` for a `List<T>` has the following member functions:

 - `size()`: returns the number of elements.
 - `empty()`: returns `true` if there are 0 elements, `false` otherwise.
 - `operator[]`: returns a `Deserialiser<T>` (or `T` for scalar `T`) for an element at the specified index. The index must be in the range `[0, size())`.
 - `at()`: like `operator[]` but throws `std::out_of_range` if the index is out of bounds.
 - `elements()`: returns a view of that yields `Deserialiser<T>` (or `T` for scalar `T`) for each element.

### Optional

`Optional<T>` is a type which may contain 0 or 1 instances of `T`.

`SerialiseSource` for an `Optional<T>` is simply an `std::optional` for a `SerialiseSource<T>`.

`Deserialiser` for an `Optional<T>` has the following member functions:
 - `has_value()`: returns `true` if an instance of `T` is contained, otherwise it returns `false`.
 - `operator*`: returns a `Deserialiser<T>` (or `T` for scalar `T`). May only be called if `has_value() == true`.
 - `value()`: like `operator*`, but throws `std::bad_optional_access` if `has_value() == false`.

### Structs

Serialise++ supports user-defined structs via the `SerialisableStruct<Fields...>` class template.
By inheriting from or aliasing `SerialisableStruct`, an automatically serialisable struct is declared via template metaprogramming.

Fields of a `SerialisableStruct` are specified with the `Field<Name, T>` class template.
`Field`'s `Name` template argument is a string literal specifying the name of the field (which must be unique within 1 `SerialisableStruct`). `Field`'s `T` template argument is the type of the field data.

For example:

```c++
struct MyStruct : SerialisableStruct<
    Field<"foo", std::int32_t>,
    Field<"bar", Optional<std::uint64_t>>,
    Field<"qux", List<std::int8_t>>
> {};
```

Note that a `SerialisableStruct` instance does not actually have any data members and so isn't usable as a normal struct.
The fields are solely for informing Serialise++ what fields to serialise.

`SerialiseSource` for a `SerialisableStruct` is a tuple-like type which holds a `SerialiseSource` for each field's type.  
It can be constructed from an initializer-list of elements like `std::tuple`.
It has the member function `get<Name>()` which gets a reference to a field by name.

`Deserialiser` for a `SerialisableStruct` has the member function `get<Name>()` which gets a `Deserialiser` (or scalar value, for scalar types) for a field by name.

## Real-world Example

Here's a more "real world" use case of serialising and deserialising, with various nesting of different types.

```c++
struct Date : SerialisableStruct<
    Field<"year", std::uint16_t>,
    Field<"month", std::uint8_t>,
    Field<"day", std::uint8_t>
> {};

struct StockRecord : SerialisableStruct<
    Field<"date", Date>,
    Field<"price", float>,
    Field<"is_open", bool>
> {};

struct StockHistory : SerialisableStruct<
    Field<"instrument_id", std::uint64_t>,
    Field<"records", List<StockRecord>>
> {};

SerialiseSource<StockHistory> const source{
    314'159'265ull,         // instrument_id
    {{                      // records
        {                   // records[0]
            {2020, 2, 25},  // date
            22517.10f,      // price
            true            // is_open
        },
        {                   // records[1]
            {2020, 2, 26},  // date
            22504.50f,      // price
            false           // is_open
        }
    }}
};
SerialiseBuffer buffer;
serialise(source, buffer);

Deserialiser<StockHistory> const stock_history = deserialise<StockHistory>(buffer.span());
std::uint64_t const instrument_id = stock_history.get<"instrument_id">();
Deserialiser<List<StockRecord>> const records = stock_history.get<"records">();
for (Deserialiser<StockRecord> const record : records.elements()) {
    Deserialiser<Date> const date = record.get<"date">();
    float const price = record.get<"price">();
    bool const is_open = record.get<"is_open">();
    // Do something with the record data...
}
```

## Error Handling

During serialisation, there should be no errors, besides something critical like out-of-memory.
Internal error scenarios are checked with `assert()`, but these should never occur unless you are using Serialise++ incorrectly or there is a bug.

During deserialisation, runtime errors can occur if the bytes buffer is malformed, which could conceivably occur if you are retrieving the data from an untrusted/unreliable source.
These cause instances of `DeserialiseError` to be thrown. `DeserialiseError` has the following type hierarchy:

 - `DeserialiseError` (abstract)
   - `BufferSizeError` (abstract): Indicates that the provided buffer is too small to contain the requested type.
     - `FixedBufferSizeError`: Indicates that the provided fixed data buffer is too small to contain the requested type.
     - `VariableBufferSizeError`: Indicates that the provided variable data buffer is too small to contain the requested type (or alternatively, a variable data offset is out of range).

These exceptions may be thrown at any time when constructing a `Deserialiser` instance or deserialising.

## Advanced Usage

TODO

(Just read the source code and comments in the meantime.)
