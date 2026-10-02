#pragma once

#include "aiws/document.hpp"
#include "aiws/processing_types.hpp"

#include <cstddef>
#include <vector>

namespace aiws {

class ChunkingStrategy {
public:
    virtual ~ChunkingStrategy() = default;

    virtual std::vector<Chunk> chunk(const Document&,
                                     std::size_t) const = 0;
};

}  // namespace aiws
