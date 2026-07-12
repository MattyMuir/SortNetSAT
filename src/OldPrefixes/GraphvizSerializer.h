#pragma once
#include <fstream>
#include <vector>
#include <map>
#include <span>

#include <sortnetutils.h>

class PrefixGraph;

class GraphvizSerializer
{
protected:
	struct RGB
	{
		uint8_t r, g, b;
	};

public:
	GraphvizSerializer(const PrefixGraph& graph_, const std::string& filename);

	void Serialize(bool vertexColors, bool edgeColors);

protected:
	const PrefixGraph& graph;
	std::ofstream file;
	std::map<std::vector<Network>, RGB> prefixColors;

	void WriteVertexLabels();
	static RGB HSVToRGB(double h, double s, double v);
	static RGB RandomColor();
	static std::string ToHex(RGB color);
	RGB GetPrefixColor(const std::vector<Network>& prefix);
	RGB GetVertexColor(size_t idx);
};