#include "GraphvizSerializer.h"

#include <random>
#include <format>

#include "PrefixGraph.h"

GraphvizSerializer::GraphvizSerializer(const PrefixGraph& graph_, const std::string& filename)
	: graph(graph_), file(filename) {}

void GraphvizSerializer::Serialize()
{
	// Write header
	file << "digraph G {\n";

    // Vertex labels
    WriteVertexLabels();

    // Vertex colors
    for (size_t idx = 0; idx < graph.nextVertexIdx; idx++)
    {
        const PrefixGraph::Prefix& fullPrefix = graph.idxToPrefix[idx];
        std::vector<Network> prefix{ fullPrefix.begin(), fullPrefix.begin() + fullPrefix.size() - 1 };
        RGB color = GetPrefixColor(prefix);
        file << std::format("v{} [style=filled fillcolor=\"{}\"]\n", idx, ToHex(color));
    }

	// Save edges
	for (PrefixGraph::Vertex* v0 : graph.idxToVertex)
		for (PrefixGraph::Vertex* v1 : v0->outgoing)
			file << std::format("v{} -> v{}\n", v0->idx, v1->idx);

    // Write footer
	file << "}";
}

void GraphvizSerializer::WriteVertexLabels()
{
    for (size_t idx = 0; idx < graph.nextVertexIdx; idx++)
    {
        const PrefixGraph::Prefix& prefix = graph.idxToPrefix[idx];
        std::string label;
        for (size_t li = 0; li < prefix.size(); li++)
        {
            if (li) label += "\\n";
            label += std::format("{}", prefix[li]);
        }

        file << std::format("v{} [label=\"{}\"]\n", idx, label);
    }
}

GraphvizSerializer::RGB GraphvizSerializer::HSVToRGB(double h, double s, double v)
{
    double hh, p, q, t, ff;
    long i;
    double r, g, b;

    if (!s)
        return RGB{ (uint8_t)(v * 255), (uint8_t)(v * 255), (uint8_t)(v * 255) };

    hh = h;
    if (hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

    switch (i)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;

    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    case 5:
    default:
        r = v;
        g = p;
        b = q;
        break;
    }
    return RGB{ (uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255) };
}

GraphvizSerializer::RGB GraphvizSerializer::RandomColor()
{
	static std::mt19937_64 gen{ std::random_device{}() };
	std::uniform_real_distribution<double> hDist{ 0.0, 360.0 };
	double h = hDist(gen);
	return HSVToRGB(h, 0.4, 1.0);
}

std::string GraphvizSerializer::ToHex(RGB color)
{
    return std::format("#{:0>2x}{:0>2x}{:0>2x}", color.r, color.g, color.b);
}

GraphvizSerializer::RGB GraphvizSerializer::GetPrefixColor(const std::vector<Network>& prefix)
{
	if (!prefixColors.contains(prefix))
		prefixColors.emplace(prefix, RandomColor());
	return prefixColors.at(prefix);
}