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

#define DO_ASSERT

Halide::Image<uint32_t> sumImageRow(Halide::Image<uint8_t>& input)
{
	Var x("x"), y("y"), c("c");
	Func f;
	RDom r(0, input.width());
	f(y, c) = sum(cast<uint32_t>(input(r, y, c)));
	f.trace_stores();
	f.parallel(y);
	//f.trace_stores();
	Halide::Image<uint32_t> output = f.realize(input.height(), 3);


#ifdef DO_ASSERT
	// assert correctness
	for (int j = 0; j < input.height(); j++)
	{
		uint32_t c[3] = { 0 };
		for (int i = 0; i < input.width(); i++)
		{
			for (int k = 0; k < 3; k++)
			{
				c[k] += input(i, j, k);
			}
		}
		for (int k = 0; k < 3; k++)
		{
			//printf("(%d, %d) = %d\n", j, k, c[k]);
			assert(c[k] == output(j, k));
		}
	}
#endif // DO_ASSERT

	return output;
}


int main(int argc, char **argv) {
	Halide::Image<uint8_t> input[3];
	char filename[256];
	for (int i = 0; i < 3; ++i)
	{
		sprintf_s(filename, "pics/%d.png", i+1);
		input[i] = load_image(filename);
	}

	auto sum1 = sumImageRow(input[0]);
	printf("%d, %d\n", sum1.width(), sum1.height());

	cout << "Success!" << endl;

	return 0;
}
