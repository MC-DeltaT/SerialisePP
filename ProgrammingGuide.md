# Serialise++ Programming Guide

## Setup

Include `serialpp/serialpp.hpp` to pull in all functionality.

All Serialise++ code entities live in the `serialpp` namespace. The `serialpp::` namespace specifier is omitted in many places in this guide for brevity.

## Serialisation Basics

The serialisation process is, conceptually, a function which maps C++ values (input) to raw bytes (output).  
In Serialise++, the code entities associated with this process are:

- `serialise_source<T>` type - contains the C++ values.
- `serialise<T>()` - performs the serialisation.
- `serialise_buffer` concept - storage for the raw bytes.

Here, `T` is the type we are interested in serialising.

Let's take a look at an example of serialising a list of `std::uint64_t`:

```c++
// Set up what value we want to serialise.
serialpp::serialise_source<serialpp::dynamic_array<std::uint64_t>> source{{42, 314, 13, 777}};
// Declare storage for the serialised bytes. basic_buffer is the standard serialise_buffer implementation, and uses heap storage.
serialpp::basic_buffer buffer;
// Perform the serialisation.
serialpp::serialise(source, buffer);
// Get a view of the resulting bytes. const_bytes_span is just an alias to std::span<std::byte const>.
serialpp::const_bytes_span bytes_view = buffer.span();
```

That's it! `buffer` now contains the bytes representation of the list of values [42, 314, 13, 777]. We obtain a view of those bytes by using `buffer.span()`.

Even with more complex types, the process is the same. `serialise_source<T>` holds the value to be serialised, `serialise<T>()` does the serialisation, and `basic_serialise_buffer` holds the result.

One important thing to note is that `serialise_source<T>` need not store the complete value to be serialised. It may instead store some form of generator for the value (e.g. a lazily-evaluated range, for the `dynamic_array` type). This enables serialisation without excessive copying of data.

(You may be wondering why `serialise_buffer` exists - why not just return, say, a `std::vector<std::byte>` from `serialise()`? The reason is performance. Memory allocations are slow and we'd like to avoid them.)

## Deserialisation Basics

The deserialisation process is similar to the serialisation process, but in reverse.  
The Serialise++ code entities associated with it are:

- `deserialiser<T>` - obtains C++ values from a buffer of bytes.
- `deserialise<T>()` - builds a `deserialiser<T>` from a view of bytes.

Again, `T` is the type we want to deserialise (note that you must know this in advance, Serialise++ does not provide any built-in way to query what type is held by a bytes buffer).

Example code for deserialising a list of `std::uint64_t`:

```c++
// The buffer with the bytes representation.
serialpp::const_bytes_span bytes = ...;
// Obtain a deserialiser object. (I'm showing the exact deserialiser type here, but usually it's nice to use auto instead.)
serialpp::deserialiser<serialpp::dynamic_array<std::uint64_t>> deser = serialpp::deserialise<serialpp::dynamic_array<std::uint64_t>>(bytes);
// Deserialise the elements and do something with them.
for (std::uint64_t element : deser.elements()) {
    do_something(element);
}
```

It's important to note that the deserialisation didn't occur until an element was iterated from `deser`. This on-the-fly deserialisation is significant for large types, where it avoids copying a large value into a single large C++ object, and then copying again what you need from that object.

Also note that the functionality of `deserialiser<T>` dependends on `T`. Different types have different member functions for deserialisation. Particularly, `deserialiser` for more complex types can produce more `deserialiser` instances for the contained data.

## Automatic Deserialisation

In the above example, when deserialising the list of `std::uint64_t`, we didn't get a `deserialiser<std::uint64_t>` for each element - instead we got the deserialised `std::uint64_t` value.

The reason for this is that, certain basic types support "automatic deserialisation" in many contexts within Serialise++, for ease of use. If a compound type `C` contains an automatically deserialisable type `A`, then `deserialiser<C>` won't give you a `deserialiser<A>`, it will just give you the value of `A` already deserialised.

Automatic deserialisation is enabled with the `enable_auto_deserialise<T>` variable template.  
The type returned when deserialising is given by `deserialise_t<T>`, which is either `deserialiser<T>` or `T`'s deserialised value type.

## Type Support

Serialise++ provides types to support more complex data. Each type is specialised for `serialise_source`, `serialiser`, and `deserialiser` to provide the required functionality.

### Scalars

Scalars in Serialise++ are the basic "atomic" types. These currently include:

- Unsigned integers
- Signed integers
- `bool`
- `std::byte`
- `float` and `double` (supported only if they are IEEE-754 binary32 and binary64, respectively)
- `null` (empty type, similar to `void`)

`serialise_source` for a scalar is a transparent wrapper around the scalar itself, stored in the data member `value`. It's constructible from a scalar value and implicitly convertible to the scalar value.

`deserialiser` for a scalar provides a member function `value()` which deserialises and returns the scalar value.

All scalar types support automatic deserialisation.

### static_array

`static_array<T, Size>` is a type which contains an ordered sequence of exactly `Size` elements of type `T`.

`serialise_source` for an `static_array<T, Size>` contains a data member `elements` of type `serialise_source<T>[Size]` if `Size > 0`, or nothing if `Size == 0`.
It can be initialised like an `std::array`:

```c++
serialise_source<static_array<long, 4>> source{{1, 2, 3, 4}};
```

`deserialiser` for an `static_array<T, Size>` has the following member functions:

- `size()`: returns the number of elements (always `Size`).
- `operator[](index)`: returns a `deserialise_t<T>` for the element at the specified index. The index must be in the range `[0, Size)`.
- `at(index)`: like `operator[]` but throws `std::out_of_range` if the index is out of bounds.
- `get<Index>()`: like `operator[]`, but checks the index at compile time.
- `elements()`: returns a view that yields `deserialise_t<T>` for each element.

The deserialiser is also destructurable into its `Size` elements using structured bindings.

### dynamic_array

`dynamic_array<T>` is a type which contains a variable-size ordered sequence of `T`.

`serialise_source` for a `dynamic_array<T>` wraps a C++20 range whose elements are convertible to `serialise_source<T>`. It may be constructed as follows:

```c++
// Default construct to contain no elements:
serialise_source<dynamic_array<long>> source;

// Construct from a braced initialiser of serialise_source<T>:
serialise_source<dynamic_array<long>> source{{1, 2, 3}};

// Construct to view (not own) a range:
std::vector<int> vec{1, 2, 3};
serialise_source<dynamic_array<long>> source{vec};

// Construct to own (by moving) a range:
std::vector<int> vec{1, 2, 3};
serialise_source<dynamic_array<long>> source{std::move(vec)};

// Construct to own a view:
auto v = std::ranges::views::iota(1, 4);
serialise_source<dynamic_array<long>> source{v};
```

`deserialiser` for a `dynamic_array<T>` has the following member functions:

- `size()`: returns the number of elements.
- `empty()`: returns `true` if there are zero elements, `false` otherwise.
- `operator[](index)`: returns a `deserialise_t<T>` for an element at the specified index. The index must be in the range `[0, size())`.
- `at(index)`: like `operator[]` but throws `std::out_of_range` if the index is out of bounds.
- `elements()`: returns a view that yields `deserialise_t<T>` for each element.

### optional

`optional<T>` is a type which may contain zero or one instances of `T`.

`serialise_source` for an `optional<T>` is simply an `std::optional` for a `serialise_source<T>`.

`deserialiser` for an `optional<T>` has the following member functions:

- `has_value()`: returns `true` if an instance of `T` is contained, otherwise it returns false`.
- `operator*()`: returns a `deserialise_t<T>`. May only be called if `has_value() == true`.
- `value()`: like `operator*`, but throws `std::bad_optional_access` if `has_value() == false`.

### variant

`variant<Ts...>` is a type which contains an instance of any type in `Ts`. `Ts` may be empty.

`serialise_source` for a `variant<Ts...>` is simply a `std::variant<serialise_source<Ts>...>`. If `Ts` is empty, then a `std::variant<std::monostate>` (since `std::variant` cannot have zero types).

`deserialiser` for a `variant<Ts...>` has the following member functions:

- `index()`: returns the zero-based index of the contained type. (Only if `Ts` is not empty.)
- `get<Index>()`: gets a `deserialise_t` for the contained type if `Index == index()`, otherwise throws `std::bad_variant_access`.
- `visit(func)`: invokes a function with a `deserialise_t` for the contained type as the argument.

### pair

`pair<T1, T2>` is a type which contains an instance of `T1` and an instance of `T2`.

`serialise_source` for a `pair<T1, T2>` is simply an `std::pair` of `serialise_source<T1>` and `serialise_source<T2>`.

`deserialiser` for a `pair<T1, T2>` has the following member functions:

- `first()`: returns a `deserialise_t<T1>`.
- `second()`: returns a `deserialise_t<T2>`.
- `get<Index>()`: returns `first()` for `Index == 0`, and `second()` for `Index == 1`.

The deserialiser is also destructurable into its two elements using structured bindings.
The first binding is to the result of `first()`, and the second binding is to the result of `second()`.

### tuple

`tuple<Ts...>` is a heterogeneous collection of any number (including zero) of types.

`serialise_source` for a `tuple<Ts...>` is simple an `std::tuple<serialise_source<Ts>...>`.

`deserialiser` for a `tuple<Ts...>` has the member function `get<Index>()` which gets a `deserialise_t` for an element by index.

The deserialiser is also destructurable into its elements using structured bindings.

### Records

Serialise++ supports user-defined record (struct) types via the `record<Args...>` class template. By inheriting from or aliasing `record`, an automatically serialisable record type is declared via template metaprogramming.

Fields of a `record` are specified with the `field<Name, T>` class template. `field`'s `Name` template argument is a string literal specifying the name of the field (which must be unique within the same `record` type). `field`'s `T` template argument is the type of the field data.

Single inheritance is supported via the `base<T>` tag. Fields from the base `record` will be prepended to the declared fields of the derived `record`.

`Args...` is a sequence of `field`, optionally starting with a `base`.

For example, with no inheritance:

```c++
struct my_record : record<
    field<"foo", std::int32_t>,
    field<"bar", optional<std::uint64_t>>,
    field<"qux", dynamic_array<std::int8_t>>
> {};
```

Example with inheritance:

```c++
struct my_derived_record : record<
    base<my_record>,
    field<"extra", float>
> {};
```

Note that a `record` instance does not actually have any data members and so isn't usable as a normal struct. The fields are solely for informing Serialise++ what to serialise.

`serialise_source` for a `record` is a tuple-like type which holds a `serialise_source` for each field's type.  
It can be constructed from an initializer-list of elements, similar to `std::tuple`. It has the member functions `get<Name>()` and `get<Index>()` which get a reference to a field by name or index.

`deserialiser` for a `record` has the member functions `get<Name>()` and `get<Index>()` which get a `deserialise_t` for a field by name or index. The deserialiser is also destructurable into its fields using structured bindings.  
A deserialiser is implicitly convertible to a deserialiser for any of the base `record`s in the inheritance hierarchy.

## Real-world Example

Here's a more "real world" use case of serialising and deserialising, with various nesting of different types.

```c++
struct date_t : record<
    field<"year", std::uint16_t>,
    field<"month", std::uint8_t>,
    field<"day", std::uint8_t>
> {};

struct stock_record : record<
    field<"date", date_t>,
    field<"price", float>,
    field<"is_open", bool>
> {};

struct stock_history : record<
    field<"instrument_id", std::uint64_t>,
    field<"records", dynamic_array<stock_record>>
> {};

serialise_source<stock_history> const source{
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
basic_buffer buffer;
serialise(source, buffer);

deserialiser<stock_history> history = deserialise<stock_history>(buffer.span());
std::uint64_t instrument_id = history.get<"instrument_id">();
deserialiser<dynamic_array<stock_record>> records = history.get<"records">();
for (deserialiser<stock_record> record : records.elements()) {
    deserialiser<date_t> date = record.get<"date">();
    float price = record.get<"price">();
    bool is_open = record.get<"is_open">();
    // Do something with the record data...
}
```

## Error Handling

During serialisation, runtime errors can occur if the object to be serialised is somehow not suitable for the Serialise++ format. These exceptions inherit from `serialise_error`, with the following hierarchy:

- `serialise_error` (abstract)
  - `object_size_error`: Occurs when an object is too big to be serialised.

Memory allocation errors may also occur, particularly from within `serialise_buffer` implementations, but these are typically unrecoverable.

During deserialisation, runtime errors can occur if the bytes buffer is malformed, which could conceivably occur if you are retrieving the data from an untrusted/unreliable source. These scenarios cause instances of `deserialise_error` to be thrown, which has the following type hierarchy:

- `deserialise_error` (abstract)
  - `buffer_bounds_error`: Occurs when deserialisation would require out-of-bounds buffer access. This may be a result of a buffer that is too small, or a bad variable data offset.

These exceptions may be thrown at any time when constructing a `deserialiser` instance or deserialising its data.

Function preconditions and internal error scenarios may be checked with `assert()`. Generally such assertions should never fail unless you are using Serialise++ incorrectly or there is a bug.

## Advanced Usage

TODO

(Just read the source code and comments in the meantime.)
