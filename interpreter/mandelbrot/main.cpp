
#include <iostream>
#include <chrono>

/*
* This example uses the stb_image library to output a PNG file. All
* required code is included in this repository for convenience.
* The project's full code can be found in the repository here:
* https://github.com/nothings/stb/
*/

/*
* This example tries to follow the structure of the example provided by
* "the AssemblyScript Book" from here:
* https://www.assemblyscript.org/examples/mandelbrot.html
*/

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "../interpreter/interpreter.h"
#include "../interpreter/introspection.h"
#include "../interpreter/error.h"

union Color {
	uint32_t packed;
	struct {
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a;
	};
};

std::vector<Color> computeColors() {
	// Computes a nice set of colors using a gradient

	auto gradientLength = 2048;

	std::vector<Color> gradient;
	gradient.reserve(gradientLength);

	auto addColorStop = [&](double stopPercent, Color stopColor) {
		if (stopPercent == 0.0) {
			gradient.resize(0);
			gradient.push_back(stopColor);
			return;
		}
		
		auto startIdx = gradient.size();
		auto stopIdx = (size_t)(gradientLength * stopPercent);
		auto startColor = gradient[startIdx - 1];
		auto lerpLength = stopIdx - startIdx;

		for (size_t i = startIdx; i < stopIdx; i++) {
			auto progress = (double)(i- startIdx) / (double)lerpLength;
			gradient.push_back(Color{
				.b = (uint8_t)std::lerp(startColor.b, stopColor.b, progress),
				.g = (uint8_t)std::lerp(startColor.g, stopColor.g, progress),
				.r = (uint8_t)std::lerp(startColor.r, stopColor.r, progress)
				});
		}
	};

	// This is supposed to resemble the JS canvas gradient API used in the 
	// original example
	addColorStop(0.00, Color{ .packed = 0x000764 });
	addColorStop(0.16, Color{ .packed = 0x2068CB });
	addColorStop(0.42, Color{ .packed = 0xEDFFFF });
	addColorStop(0.6425, Color{ .packed = 0xFFAA00 });
	addColorStop(0.8575, Color{ .packed = 0x000200 });
	addColorStop(1.0000, Color{ .packed = 0x000000 });

	return gradient;
}

int main() {
	std::cout << "Running Mandelbrot example\n";

	auto imageChannels = 3;
	auto imageHeight = 1000;
	auto imageWidth = 1000;

	auto imagePixels = std::make_unique<uint8_t[]>(imageHeight * imageWidth * imageChannels);

	// Discrete color indices in range [0, 2047] (2 bytes per pixel)
	auto numMemoryBytes = (imageWidth * imageHeight) * 2;
	auto numMemoryPages = ((numMemoryBytes+ 0xffff) & ~0xffff) >> 16;

	auto colors = computeColors();

	try {
		using WASM::f64, WASM::i32;
		WASM::Interpreter interpreter;

		// Get time to measure parse time (disable logging for usable numbers)
		auto parseTime = std::chrono::high_resolution_clock::now();

		// Add a console logger. This is very noise, but insightfull
		auto logger = std::make_unique<WASM::ConsoleLogger>( std::cout );
		interpreter.attachIntrospector(std::move(logger));
		
		// Define a host module that provides everything that the wasm module needs
		WASM::HostModuleBuilder envModuleBuilder{ "env" };
		envModuleBuilder
			.defineFunction("Math.log", [&](f64 x) { return std::log(x); })
			.defineFunction("Math.log2", [&](f64 x) { return std::log2(x); })
			.defineMemory("memory", numMemoryPages);

		auto envModule = interpreter.registerHostModule(envModuleBuilder);

		interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/mandelbrot/build/release.wasm");
		interpreter.compileAndLinkModules();

		interpreter.runStartFunctions();

		// Run the function and measure the time
		auto startTime = std::chrono::high_resolution_clock::now();

		auto updateFunction= interpreter.functionByName("release", "update");
		auto result = interpreter.runFunction(updateFunction, (i32)imageWidth, (i32)imageHeight, (i32)40);

		auto endTime = std::chrono::high_resolution_clock::now();

		// Print the function's return value and the run time
		result.print(std::cout);
		std::cout << "Load time: " << std::chrono::duration_cast<std::chrono::milliseconds>(startTime - parseTime) << " (read, parse, link, compile)" << std::endl;
		std::cout << "Run time: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime) << std::endl;

		// Access the memory view of the host module
		auto memory= envModule.hostMemoryByName("memory");
		auto memoryView= memory->memoryView<WASM::u16>();

		assert(memoryView.size() >= imageWidth* imageHeight);
		
		// Convert the calculated values to an image using the
		// gradient lookup table
		auto index = 0;
		for (auto y = 0; y < imageHeight; y++)
		{
			auto yx = y * imageWidth;
			for (auto x = 0; x < imageWidth; x++)
			{
				auto value = memoryView[yx + x];
				auto color = colors[value];
				imagePixels[index++] = color.r;
				imagePixels[index++] = color.g;
				imagePixels[index++] = color.b;
			}
		}
	}
	catch (WASM::Error& e) {
		std::cerr << "\n\n========================================\n" << std::endl;
		std::cerr << "Caught wasm error: " << e << std::endl;
	}
	catch (std::exception& e) {
		std::cerr << "\n\n========================================\n" << std::endl;
		std::cerr << "Caught generic error: " << e.what() << std::endl;
	}

	stbi_write_png("output.png", imageWidth, imageHeight, imageChannels, imagePixels.get(), imageWidth * imageChannels);
	return 0;
}
