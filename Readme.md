# conjure

A clean C++ stackful coroutine library just like conjury.

**Currently under development, ideas and suggestions are welcome! :)**

## Requirement

- Platform: Linux or MacOS
- Compiler: whichever supports C++17

## Where to start

See [examples](https://github.com/VinaLx/conjure/tree/master/examples)

## Build

```shell
pwd # should prints 'path/to/conjure'
mkdir build
cmake ..
make -j
ls ../bin # all build outputs are in 'conjure/bin'
```

## Documentation

TODO

### FAQ

#### How does a coroutine start?

- The coroutine is resumed by another coroutine.
- The coroutine is waited by another coroutine.

#### When would a coroutine become un-executable?

- The coroutine explicitly suspended.
- The coroutine waits another coroutine to exit.
- The coroutine waits another coroutine to generate value.

#### When is return target assigned?

- A coroutine explicitly waits another coroutine.
- A coroutine resumes another coroutine.
- A coroutine waits the generator value of another coroutine.

#### When would scheduler involve?

- A non-main coroutine without a return target exits. (IMPOSSIBLE)
- A coroutine without a return target yield. (CURRENTLY IMPOSSIBLE)
- A coroutine waits another coroutine who is not executable.
