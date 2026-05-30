#pragma once
#include <vector>

class OutputSet
{
public:
	OutputSet(uint8_t n);
	OutputSet(uint8_t n, size_t reserve);

	void Insert(uint64_t output);

	OutputSet& operator=(std::vector<uint64_t>&& outputs_);
	bool operator==(const OutputSet& other) const;

	const uint64_t* begin() const;
	const uint64_t* end() const;

	size_t Size() const;
	bool IsEmpty() const;
	bool Contains(uint64_t x) const;

protected:
	std::vector<uint64_t> outputs;
	std::vector<bool> containsOutput;
};