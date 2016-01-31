/************************************************************************/
/* ImageMatchMerge:
match image from head to tail and merge them
*/
/************************************************************************/

#include "stdafx.h"
#include "ImageMatchMerge.h"

using namespace std;
using namespace Halide;
using namespace Halide::Tools;

Halide::Image<uint32_t> ImageMatchMerge::sumImageRow(const Halide::Image<uint8_t>& input)
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
			assert(c[k] == output(k, j));
		}
	}
#endif // DO_ASSERT

	return output;
}

Halide::Image<uint32_t> ImageMatchMerge::sumImageRowBlock(
	const Halide::Image<uint8_t>& input, 
	int block_width)
{
	int block = input.width() / block_width;
	if (input.width() > block * block_width)
		++block;

	Image<uint32_t> output(block, input.height(), input.channels());

	for (int j = 0; j < input.height(); j++)
	{
		for (int b = 0; b < block - 1; ++b)
		{
			uint32_t c[3] = { 0 };
			for (int i = b*block_width; i < block_width*(b + 1); ++i)
			{
				for (int k = 0; k < 3; k++)
				{
					c[k] += input(i, j, k);
				}
			}
			for (int k = 0; k < 3; k++)
			{
				output(b, j, k) = c[k];
			}
		}

	}

	return output;
}

std::tuple<int, int> ImageMatchMerge::findHeadAndTail(const Halide::Image<uint32_t>& sum1,
	const Halide::Image<uint32_t>& sum2)
{
	int height = min(sum1.height(), sum2.height());
	int head = 0, tail = height - 1;
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

	return tuple<int, int>(head, tail);
}

std::tuple<int, int> findHeadAndTail2(const Halide::Image<uint32_t>& sum1,
	const Halide::Image<uint32_t>& sum2)
{
	const int height = min(sum1.height(), sum2.height());
	const int width = min(sum1.width(), sum2.width());

	int head = 0, tail = height - 1;
	for (; head < height; ++head)
	{
		int match = 0;
		for (int x = 0; x < width; ++x)
		{
			if (sum1(x, head, 0) == sum2(x, head, 0) &&
				sum1(x, head, 1) == sum2(x, head, 1) &&
				sum1(x, head, 2) == sum2(x, head, 2))
				++match;
		}

		if ((float)match / width < 0.5)
		{
			--head;
			break;
		}
	}

	for (; tail >= 0; --tail)
	{
		int match = 0;
		for (int x = 0; x < width; ++x)
		{
			if (sum1(x, tail, 0) == sum2(x, tail, 0) &&
				sum1(x, tail, 1) == sum2(x, tail, 1) &&
				sum1(x, tail, 2) == sum2(x, tail, 2))
				++match;
		}

		if ((float)match / width < 0.9)
		{
			++tail;
			break;
		}
	}

	tail = sum1.height() - 1 - tail;

	return tuple<int, int>(head, tail);
}

template<typename T>
Halide::Image<T> ImageMatchMerge::cutHeadAndTail(const Halide::Image<T>& input, int headLen, int tailLen)
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

template<typename T>
inline float calcAvgMatch(const Halide::Image<T>& top, const Halide::Image<T>& down, int offset)
{
	const int height = min(top.height() - offset, down.height());
	const int width = min(top.width(), down.width());
	int match = 0;
	for (int y = 0; y < height; ++y, ++offset)
	{
		for (int x = 0; x < width; ++x)
		{
			if (top(x, offset, 0) == down(x, y, 0) &&
				top(x, offset, 1) == down(x, y, 1) &&
				top(x, offset, 2) == down(x, y, 2))
			{
				++match;
			}
		}

	}
	return float(match) / (height * width);
}

template<typename T>
int ImageMatchMerge::avgMatchImages(const Halide::Image<T>& top, const Halide::Image<T>& down)
{
	const int width = min(top.width(), down.width()), height = min(top.height(), down.height());

	int res = 0;
	float maxm = 0;
	for (int h = 1; h <= height; ++h)
	{
		float avgm = calcAvgMatch(top, down, height - h);
		if (avgm >= maxm)
		{
			maxm = avgm;
			res = max(res, h);
		}
	}

	return res;
}

bool ImageMatchMerge::run()
{
	
	clock_t begin = clock(), end = 0;
	float elapsed_time = 0;

	const int num = m_image_files.size();

	if (num <= 0)
		return false;

	vector<Halide::Image<uint8_t> > input(num);
	vector<Halide::Image<uint32_t> > sums(num);

	// load all image 
	for (int i = 0; i < num; ++i)
	{
		input[i] = load_image(m_image_files[i]);
	}

	printf("channels %d\n", input[0].channels());

	end = clock();
	elapsed_time = float(end - begin) / CLOCKS_PER_SEC;
	printf("load time = %f\n", elapsed_time);
	begin = clock();

	// sum image block 
	for (int i = 0; i < num; ++i)
	{
		//sums[i] = sumImageRow(input[i]);
		sums[i] = sumImageRowBlock(input[i], 10);
	}

	end = clock();
	elapsed_time = float(end - begin) / CLOCKS_PER_SEC;
	printf("sum image block time = %f\n", elapsed_time);
	begin = clock();

	const int width = input[0].width(), height = input[0].height(), channel = input[0].channels();

	int head = height, tail = height;
	for (int i = 0; i < num - 1; ++i)
	{
		auto ht = findHeadAndTail2(sums[0], sums[1]);

		head = min(head, get<0>(ht));
		tail = min(tail, get<1>(ht));
	}

	printf("head = %d, tail = %d\n", head, tail);

	assert(tail + head < height);

	end = clock();
	elapsed_time = float(end - begin) / CLOCKS_PER_SEC;
	printf("findHeadAndTail time = %f\n", elapsed_time);
	begin = clock();

	// cut head and tail
	vector<Halide::Image<uint8_t> > cuts(num);
	vector<Halide::Image<uint32_t> > cut_sums(num);
	for (int i = 0; i < num; ++i)
	{
		cuts[i] = cutHeadAndTail(input[i], head, tail);
		//cut_sums[i] = cutHeadAndTail(sums[i], head, tail);
		cut_sums[i] = sumImageRowBlock(cuts[i], 20);

		//sprintf_s(filename, "out%d.png", i);
		//save_image(cuts[i], filename);
	}

	end = clock();
	elapsed_time = float(end - begin) / CLOCKS_PER_SEC;
	printf("cutHeadAndTail time = %f\n", elapsed_time);
	begin = clock();

	// find match bwtween cuts
	vector<int> match(num, 0);
	int matchall = 0;
	for (int i = 0; i < num - 1; ++i)
	{
		//match[i] = avgMatchImages(cuts[i], cuts[i + 1]);
		match[i] = avgMatchImages(cut_sums[i], cut_sums[i + 1]);
		matchall += match[i];
		cout << "match = " << match[i] << endl;
	}

	end = clock();
	elapsed_time = float(end - begin) / CLOCKS_PER_SEC;
	printf("avgMatchImages time = %f\n", elapsed_time);
	begin = clock();

	// joint all the cut images
	Func joint("joint");
	Var x("x"), y("y"), c("c");
	joint(x, y, c) = cast<uint8_t>(0);

	const int res_width = width;
	const int res_height = height * 3 - (head + tail) * 2 - matchall;

	//header
	RDom headr(0, head);
	joint(x, headr, c) = input[0](x, headr, c);

	//image
	int curh = head;
	for (int i = 0; i < num; ++i)
	{
		int imgh = cuts[0].height() - match[i];
		RDom imgr(0, imgh);
		joint(x, curh + imgr, c) = cuts[i](x, imgr, c);
		curh += imgh;
	}

	//tail
	RDom tailr(0, tail);
	joint(x, res_height - tail - 1 + tailr, c) = input[0](x, height - tail - 1 + tailr, c);

	m_result = joint.realize(res_width, res_height, channel);

	end = clock();
	elapsed_time = float(end - begin) / CLOCKS_PER_SEC;
	printf("joint time = %f\n", elapsed_time);
	begin = clock();

	return true;
}

void ImageMatchMerge::saveResult(const std::string& filename)
{
	save_image(m_result, filename);
}