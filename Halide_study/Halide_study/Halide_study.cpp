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

//#define DO_ASSERT

Halide::Image<uint32_t> sumImageRow(Halide::Image<uint8_t>& input)
{
	Var x("x"), y("y"), c("c");
	Func f;
	RDom r(0, input.width());
	f(y, c) = sum(cast<uint32_t>(input(r, y, c)));
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


tuple<int, int> findHeadAndTail(const Halide::Image<uint32_t>& sum1, const Halide::Image<uint32_t>& sum2)
{
	int width = min(sum1.width(), sum2.width());
	int head = 0, tail = width -1;
	for (; head < width; ++head)
	{
		bool is_equal = true;
		for (int i = 0; i < 3; ++i)
		{
			if (sum1(head, i) != sum2(head, i))
			{
				is_equal = false;
				break;
			}
		}
		
		if (!is_equal)
		{
			break;
		}
	}

	for (; tail >= 0; --tail)
	{
		bool is_equal = true;
		for (int i = 0; i < 3; ++i)
		{
			if (sum1(tail, i) != sum2(tail, i))
			{
				is_equal = false;
				break;
			}
		}

		if (!is_equal)
		{
			++tail;
			break;
		}
	}

	cout << width << ", " << head << ", " << tail << endl;

	return tuple<int, int>(head, tail);
}

Halide::Image<uint8_t> cutHeadAndTail(const Halide::Image<uint8_t>& input, int head, int tail)
{
	Var x("x"), y("y"), c("c");
	RDom r(0, tail - head);
	Func f;
	f(x, y, c) = input(x, y + head, c);
	Halide::Image<uint8_t> output = f.realize(input.width(), tail - head, input.channels());
	return output;
}

int main(int argc, char **argv) {
	Halide::Image<uint8_t> input[3];
	Halide::Image<uint32_t> sums[3];
	char filename[256];
	for (int i = 0; i < 3; ++i)
	{
		sprintf_s(filename, "pics/%d.png", i+1);
		input[i] = load_image(filename);
		sums[i] = sumImageRow(input[i]);
	}

// 	for (int i = 0; i < 10; ++i)
// 	{
// 		printf("sums[0][%d] = (%d, %d, %d)\n", i, sums[0](i, 0), sums[0](i, 1), sums[0](i, 2));
// 		printf("sums[1][%d] = (%d, %d, %d)\n", i, sums[1](i, 0), sums[1](i, 1), sums[1](i, 2));
// 	}

	auto ht = findHeadAndTail(sums[0], sums[1]);

	int head = get<0>(ht), tail = get<1>(ht);
	cout << get<0>(ht) << ", " << get<1>(ht) << endl;

	assert(tail > head);

	auto out0 = cutHeadAndTail(input[0], head, tail);
	save_image(out0, "out0.png");

	auto out1 = cutHeadAndTail(input[1], head, tail);
	save_image(out1, "out1.png");

	cout << "Success!" << endl;

	return 0;
}

