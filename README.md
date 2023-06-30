# A simple WebAssembly Interpreter

This is an embeddable WebAssembly interpreter written in C++20.
The aim of this project it to create an interpreter with a simple
to use interface to be used as a scripting engine for other
applications. There are no dependencies besides the C++ STL.

The project is developed with the following goals in mind
- modern readable code
- moderate memory footprint
- quick start up
- standard conform code validation

## Building

The project uses CMake and the base `CMakeLists.txt` adds three
sub-directories:

- `interpreter` The actual interpreter library to link to
- `embedder` A test application embedding the interpreter
- `mandelbrot` A test application for the mandelbrot demo
  (more down below)

Just build the static library in the `interpreter` directory
and link to it. Checkout one of the test application CMake
configuration for hints.

## Usage

For working examples checkout the `embedder` and `mandelbrot`
applications.

### Running the interpreter

The code below shows how to create a new interpreter instance,
load a module and run its start function after compilation. Note,
that after compilation no further wasm or host modules can be
loaded or registered respectively.

```C++
#include "interpreter/interpreter.h"
#include "interpreter/error.h"

int main() {
  try {
    WASM::Interpreter interpreter;
    interpreter.loadModule("path/to/myModule.wasm");
    interpreter.compileAndLinkModules();

    interpreter.runStartFunctions();

  } catch (WASM::Error& e) {
    std::cerr << "Caught wasm error: " << e << std::endl;
  }
  catch (std::exception& e) {
    std::cerr << "Caught generic error: " << e.what() << std::endl;
  }
}
```

### Run a specific function

```C++
int main() {
  WASM::Interpreter interpreter;

  /** ... Load the WASM modules ... **/

  using WASM::i32;
  auto addFunc= interpreter.functionByName("myModule", "myAddFunction");
  auto result = interpreter.runFunction(addFunc, (i32)123, (i32)234);

  result.print(std::cout);
}
``` 

### Register host modules

Create a native host module that wasm modules can link to.
This way IO operations and interfaces to the WASM code can
be implemented.

```C++
int main() {
  WASM::Interpreter interpreter;

  /** ... Load the WASM modules ... **/

  using WASM::i32, WASM::f64;

  // Define a module with some functions and a memory instance (10 pages)
  WASM::HostModuleBuilder myModuleBuilder{ "myNativeModule" };
  myModuleBuilder
    .defineFunction("printInt", [&](i32 x) { std::cout << x << std::endl; })
    .defineFunction("floatSum", [&](f64 x, f64 y) { return x+ y; })
    .defineFunction("myLog", [&](f64 x) { return std::log(x); })
    .defineMemory("memory", 10);

  // Register the host module before compilation
  auto myHostModule = interpreter.registerHostModule(myModuleBuilder);

  // Load a wasm module that now can link to the 'myNativeModule' module
  interpreter.loadModule("path/to/myModule.wasm");
  
  interpreter.compileAndLinkModules();
  interpreter.runStartFunctions();

  // Access the memory view of the host module
  auto memory= myHostModule.hostMemoryByName("memory");
  auto memoryView= memory->memoryView<i32>();

  for(auto& data : memoryView) {
    std::cout << data << std::endl;
  }
}
``` 

### Adding introspection

The operations of the interpreter can be observed by the
embedder application by attaching an introspector. The
basic `WASM::ConsoleLogger` just prints every event to the
console. However, a custom introspector can be defined by
sub-classing an implementing the abstract `WASM::Introspector`
class.

```C++
#include "interpreter/introspection.h"

int main() {
  WASM::Interpreter interpreter;

  // Attach a console logging introspector (very noisy)
  auto logger = std::make_unique<WASM::ConsoleLogger>( std::cout );
  interpreter.attachIntrospector(std::move(logger));

  /** ... Use the interpreter ... **/
}
``` 

## Mandelbrot demo

As a little demonstration the [mandelbrot demo](https://www.assemblyscript.org/examples/mandelbrot.html)
from the Assembly Script Cookbook website is included in this repository.
The code can be found in the `/webassembly/mandelbrot` directory where
it needs to be compiled with the Assembly Script compiler. You can simply
download a local installation using `npm i` in the source mandelbrot directory.

The `/interpreter/mandelbrot` C++ application serves as an
example how the interpreter can be embedded into an application.
There the mandelbrot webassembly module is loaded (check the hardcoded
path!) and the interpreter executes the `update` function
(see the Assembly Script source). The resulting image is generated
from a color gradient, encoded and stored to disk as `output.png`
using C++ code. This closely resembles the original setup 
from the Assembly Script Cookbook page where these steps are
performed using JS.

This demo requires [stb_image](https://github.com/nothings/stb/tree/master) which
is included in this repository.

## Author

This project is written by Matthis Preymann (aka PreyMa) as
part of a University course ([185.A49 UE Abstrakte Maschinen SS23](https://www.complang.tuwien.ac.at/andi/185966)
TU Vienna).

## License

This project is licensed under the MIT license.

The mandelbrot demo uses `stb_image` licensed under the MIT
license (copyright Sean Barrett) which is included in this
repository for convenience sake.
