#pragma once

#include "compile_document.hpp"

#include <filesystem>
#include <iosfwd>

namespace vscc
{
class CompileCommandsPrinter
{
public:
    void write_file(const fs::path& output_path, const CompilationDocument& document) const;
    void write(std::ostream& stream, const CompilationDocument& document) const;
};
} // namespace vscc
