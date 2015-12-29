// Halide tutorial lesson 1: Getting started with Funcs, Vars, and Exprs

// This lesson demonstrates basic usage of Halide as a JIT compiler for imaging.
// The only Halide header file you need is Halide.h. It includes all of Halide.
#include "stdafx.h"
#include "ImageMatchMerge.h"

// Include some support code for loading pngs.

int main(int argc, char **argv)
{
	ImageMatchMerge paser;
	char filename[256];
	for (int i = 1; i < 4; ++i)
	{
		sprintf_s(filename, "pics/%d.png", i);
		paser.m_image_files.push_back(filename);
	}

	paser.run();

	paser.saveResult("res.png");
}