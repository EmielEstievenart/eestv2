#pragma once

#include "cli.hpp"
#include "compile_document.hpp"
#include "diagnostics.hpp"

namespace vscc
{
struct DocumentCreationResult
{
    CompilationDocument document;
    unsigned evaluated_projects = 0;
    bool strict_failure = false;
};

class MsbuildDocumentCreator
{
public:
    DocumentCreationResult create(const CliOptions& options, Diagnostics& diagnostics) const;
};
} // namespace vscc
