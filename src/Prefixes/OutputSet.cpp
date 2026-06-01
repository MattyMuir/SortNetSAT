#include "OutputSet.h"

static inline size_t MapSize(uint8_t n)
{
	return (1ULL << n);
}

OutputSet::OutputSet(uint8_t n)
	: containsOutput(MapSize(n)) {}

OutputSet::OutputSet(uint8_t n, size_t reserve)
	: containsOutput(MapSize(n))
{
	outputs.reserve(reserve);
}

void OutputSet::Insert(uint64_t output)
{
	if (Contains(output)) return;
	outputs.push_back(output);
	containsOutput[output] = true;
}

void OutputSet::Reserve(size_t num)
{
	outputs.reserve(num);
}

OutputSet& OutputSet::operator=(std::vector<uint64_t>&& outputs_)
{
	outputs = std::move(outputs_);
	for (uint64_t output : outputs)
		containsOutput[output] = true;
	return *this;
}

bool OutputSet::operator==(const OutputSet& other) const
{
	return containsOutput == other.containsOutput;
}

const uint64_t* OutputSet::begin() const
{
	return outputs.data();
}

const uint64_t* OutputSet::end() const
{
	return outputs.data() + outputs.size();
}

size_t OutputSet::Size() const
{
	return outputs.size();
}

bool OutputSet::IsEmpty() const
{
	return outputs.empty();
}

bool OutputSet::Contains(uint64_t x) const
{
	return containsOutput[x];
}