# Serialise++ Benchmarks

## Introduction

Benchmarking the absolute performance of serialisation and deserialisation is tricky, because these operations are effectively glorified memory accesses, and memory access has many nuances with modern CPUs and optimising compilers. The performance of a memory access can be influenced significantly by the surrounding code, optimisations, CPU cache effects, and so on.

As such, it is difficult to create benchmarks that accurately capture the general performance cost of serialisation. It would be nice to measure performance in various typical usage scenarios, but the reality is that there are so many variables that the most useful benchmark is the once which tests the real application code. Of course, such a benchmark can only be implemented by the end user.

## Goals

What can be done, though, is try to provide an estimate of the lower bound on the performance cost of using Serialise++. That is, an estimate of the highest possible serialisation/deserialisation throughput, assuming "best case" usage scenarios.

Additionally, we can create a set of benchmarks which can be used to track the relative performance of serialisation/deserialisation as the library evolves.

## Benchmark Coverage

There are various axes of performance variation in Serialise++ which are useful to measure.

Obviously, we want to measure the performance of serialisation and deserialisation. I think it is most useful to measure these separately, since no real application is likely to do an immediate round-trip.

Next, I think it is natural to group benchmarks by general data type (e.g. scalar, `dynamic_array`, `pair`, etc.). Each data type has different serialisation/deserialisation code, and hence we expect the performance may differ.  

Within the same general data type, there is an axis of variation in the "complexity" of a data type. For example, `dynamic_array` may have some general performance characteristics, but what if the element type is a scalar versus a big record type? How do deeply nested types perform?

Finally, sometimes there is plausibly some variation of performance within the same data type as a result of different data values, which is handy to observe. For example, serialising a instances of `optional` which are most often empty will have different performance to when the instances are often nonempty.

## Methodology for Serialisation

To benchmark serialisation, the code iterates over an array of randomly-valued pregenerated serialise sources and serialises them into the same buffer. The resulting buffer data is not used, to avoid incurring performance overhead (plus such use is heavily application-specific). Multiple different serialise sources are used rather than a single source, since no application is likely to repeatedly serialise the same data. However only one serialise buffer is used, since this avoids expensive reallocations - a technique any high-performance application would want to employ anyway.

It is important to note that in most benchmarks the performance measurement does not include the creation of the serialise sources. The reason is that it is difficult to generate varied serialise sources in a generalised manner without incurring significant overhead. The downside of this approach is that the cost of instantiating a serialise source is mostly unmeasured. For most types I expect this cost is low in comparison to that of serialisation. For types where this assumption is likely not true, such as `dynamic_array`, some specialised benchmarks that include measurement of serialise source instantiation are in place.

## Methodology for Deserialisation

It is very difficult to quantify *just* deserialisation - that is, loading and parsing from data from memory into the CPU - without:

1. the compiler optimising away the unused loads, or else
2. incurring significant performance overhead, or else
3. resorting to platform-specific tricks.

Sadly I have no optimal solution to these issues. What the benchmark code does is try to "use" the result of deserialisation with a little overhead as possible, by summing all scalars deserialised from an object (which hopefully can be done in CPU registers for minimal cost), then writing that sum to memory once at the end.

It's important to keep this small overhead in mind when making comparisons between serialisation and deserialisation throughputs. We expect that the throughput numbers reported by the benchmarks for deserialisation are somewhat lower than for serialisations.
