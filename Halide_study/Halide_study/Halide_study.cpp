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
	f(c, y) = sum(cast<uint32_t>(input(r, y, c)));
	f.parallel(y);
	//f.trace_stores();
	Halide::Image<uint32_t> output = f.realize(3, input.height());

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
	int height = min(sum1.height(), sum2.height());
	int head = 0, tail = height -1;
	for (; head < height; ++head)
	{
		bool is_equal = true;
		for (int i = 0; i < 3; ++i)
		{
			if (sum1(i, head) != sum2(i, head))
			{
				is_equal = false;
				break;
			}
		}
		
		if (!is_equal)
		{
			--head;
			break;
		}
	}

	for (; tail >= 0; --tail)
	{
		bool is_equal = true;
		for (int i = 0; i < 3; ++i)
		{
			if (sum1(i, tail) != sum2(i, tail))
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

	tail = sum1.height() - 1 - tail;

	cout << height << ", " << head << ", " << tail << endl;

	return tuple<int, int>(head, tail);
}

template<typename T>
Halide::Image<T> cutHeadAndTail(const Halide::Image<T>& input, int headLen, int tailLen)
{
	Var x("x"), y("y"), c("c");
	const int out_height = input.height() - headLen - tailLen;
	Func f;
	if (input.channels() > 1)
	{
		f(x, y, c) = input(x, y + headLen, c);
		Halide::Image<T> output = f.realize(input.width(), out_height, input.channels());
		return output;
	}
	else
	{
		f(x, y) = input(x, y + headLen);
		Halide::Image<T> output = f.realize(input.width(), out_height);
		return output;
	}
}

int matchImages(const Halide::Image<uint32_t>& top, const Halide::Image<uint32_t>& down)
{
	const int height = min(top.height(), down.height());
	const int width = top.width();
	printf("width = %d, height = %d\n", width, height);
	int res = 0;
	for (int h = 1; h <= height; ++h)
	{
		bool match = true;
		for (int cur = 0; cur < h; ++cur)
		{
			for (int i = 0; i < width; ++i)
			{
				if (top(i, height - h + cur) != down(i, cur))
				{
					match = false;
					break;
				}
			}
			if (!match)
			{
				break;
			}
		}
		if (match)
		{
			res = max(res, h);
		}
	}

	return res;
}

int main(int argc, char **argv) {
	const int num = 3;
	Halide::Image<uint8_t> input[num];
	Halide::Image<uint32_t> sums[num];
	char filename[256];
	for (int i = 0; i < num; ++i)
	{
		sprintf_s(filename, "pics/%d.png", i+1);
		input[i] = load_image(filename);
		sums[i] = sumImageRow(input[i]);
	}

	const int width = input[0].width(), height = input[0].height(), channel=input[0].channels();

	int head = 0, tail = 0;
	for (int i = 0; i < num - 1; ++i)
	{
		auto ht = findHeadAndTail(sums[0], sums[1]);

		head = max(head, get<0>(ht));
		tail = max(tail, get<1>(ht));

	}

	printf("head = %d, tail = %d\n", head, tail);

	assert(tail + head < height);

	// cut head and tail
	Halide::Image<uint8_t> cuts[num];
	Halide::Image<uint32_t> cut_sums[num];
	for (int i = 0; i < num; ++i)
	{
		cuts[i] = cutHeadAndTail(input[i], head, tail);
		cut_sums[i] = cutHeadAndTail(sums[i], head, tail);

		sprintf_s(filename, "out%d.png", i);
		save_image(cuts[i], filename);
	}

	// find match bwtween cuts
	for (int i = 0; i < num-1; ++i)
	{
		int match = matchImages(cut_sums[i], cut_sums[i + 1]);
		cout << "match = " << match << endl;
	}
	exit(0);

	// joint all the cut images
	Func joint("joint");
	Var x("x"), y("y"), c("c");
	joint(x, y, c) = cast<uint8_t>(0);

	const int res_width = width;
	const int res_height = height * 3 - (head + tail) * 2;

	//header
	RDom headr(0, head);
	joint(x, headr, c) = input[0](x, headr, c);

	//image
	RDom imgr(0, cuts[0].height());
	for (int i = 0; i < num; ++i)
	{
		joint(x, head + cuts[0].height() * i + imgr, c) = cuts[i](x, imgr, c);
	}

	//tail
	RDom tailr(0, tail);
	joint(x, res_height - tail - 1 + tailr, c) = input[0](x, height - tail - 1 + tailr, c);


	Halide::Image<uint8_t> res = joint.realize(res_width, res_height, channel);
	save_image(res, "res.png");


	cout << "Success!" << endl;

	return 0;
}

