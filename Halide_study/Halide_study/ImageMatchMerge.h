/************************************************************************/
/* ImageMatchMerge:
	match image from head to tail and merge them
*/
/************************************************************************/

#pragma once
#include <vector>
#include <string>
#include <tuple>
#include <string.h>
#include "Halide.h"

class ImageMatchMerge
{
public:
	ImageMatchMerge() {}

	ImageMatchMerge(const std::vector<std::string>& image_files) 
	{
		m_image_files = image_files;
	}

	bool run();

	void saveResult(const std::string& filename);

	std::vector<std::string> m_image_files;

	Halide::Image<uint8_t> m_result;

private:
	Halide::Image<uint32_t> sumImageRow(const Halide::Image<uint8_t>& input);

	Halide::Image<uint32_t> sumImageRowBlock(const Halide::Image<uint8_t>& input, int block_width);

	std::tuple<int, int> findHeadAndTail(const Halide::Image<uint32_t>& sum1, 
		const Halide::Image<uint32_t>& sum2);

	template<typename T>
	Halide::Image<T> cutHeadAndTail(const Halide::Image<T>& input, int headLen, int tailLen);

	template<typename T>
	int avgMatchImages(const Halide::Image<T>& top, const Halide::Image<T>& down);
};