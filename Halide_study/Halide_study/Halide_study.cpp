// Halide tutorial lesson 1: Getting started with Funcs, Vars, and Exprs

// This lesson demonstrates basic usage of Halide as a JIT compiler for imaging.
// The only Halide header file you need is Halide.h. It includes all of Halide.
#include "stdafx.h"
#include "Halide.h"
#include <iostream>
#include <stdio.h>

// Include some support code for loading pngs.
#include "halide_image_io.h"
using namespace Halide;
using namespace Halide::Tools;
using namespace std;

int main(int argc, char **argv) {
	Halide::Image<uint8_t> input = load_image("pics/1.png");

	//save_image(input, "1.png");
	Var x("x"), y("y"), c("c");
	Func f;
	RDom r(0, input.width());
	f(y, c) = sum(cast<uint32_t>(input(r, y, c)));
	f.parallel(y);
	//f.trace_stores();

	printf("%d, %d, %d\n", input(0, 10, 0), input(0, 10, 1), input(0, 10, 2));

	Halide::Image<uint32_t> output = f.realize(3, 3);

	for (int j = 0; j < output.height(); j++) {
		for (int i = 0; i < output.width(); i++) {
			// We can access a pixel of an Image object using similar
			// syntax to defining and using functions.
			printf("(%d, %d) = %d\n", i, j, output(i, j));
		}
	}

	printf("%d, %d\n", input.width(), input.height());

	cout << "Success!" << endl;

	return 0;
}
