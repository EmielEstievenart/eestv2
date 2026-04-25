# Design Document: `vscompilecommands`

## 1. Problem statement

CMake can generate `compile_commands.json` when `CMAKE_EXPORT_COMPILE_COMMANDS` is enabled, but CMake documents that this feature is implemented only for Makefile and Ninja generators and is ignored by other generators, including Visual Studio generators. This creates a gap for projects that intentionally use the CMake Visual Studio generator but still need a compilation database for Visual Studio Code, clangd, clang-tidy, static analyzers, indexers, or other C/C++ tooling. ([CMake][1])

`vscompilecommands` fills that gap by generating a `compile_commands.json` file from Visual Studio `.sln` and `.vcxproj` inputs without requiring the user to switch away from the Visual Studio generator.

The tool must support mixed Visual Studio solutions. A solution may contain C#, C, C++, test, packaging, utility, or metadata projects. Only Visual C++ `.vcxproj` projects should be evaluated. Non-C/C++ projects must be skipped cleanly and reported only when verbose diagnostics are enabled.

The key design principle is: **MSBuild is the source of truth**. Visual Studio C++ projects are MSBuild projects, and `.vcxproj` files may import `.props` and `.targets` files, use configuration-specific property groups, item definitions, and per-file metadata. Manual XML parsing of `.vcxproj` files would be fragile because MSBuild evaluation order, imports, default metadata, and property expansion matter. ([Microsoft Learn][2])

MSBuild 17.8 and later provide command-line query options such as `-getItem` and `-getProperty`. Without specifying a target, these return evaluated project data without executing a build; `-getItem` output includes item metadata and is emitted as JSON. ([Microsoft Learn][3])

---

## 2. Goals

The tool shall:

1. Generate a valid `compile_commands.json` file.
2. Accept either a Visual Studio solution file or a Visual C++ project file:

   * `.sln`
   * `.vcxproj`
3. Support mixed Visual Studio solutions.
4. Ignore `.csproj`, `.vbproj`, `.fsproj`, `.sqlproj`, `.vdproj`, solution folders, and other non-`.vcxproj` entries.
5. Use MSBuild evaluation rather than manually interpreting `.vcxproj` XML.
6. Support explicit configuration and platform selection.
7. Choose sensible defaults when configuration or platform is omitted.
8. Generate entries only for C/C++ translation units from evaluated `ClCompile` items.
9. Respect per-file metadata such as:

   * `ExcludedFromBuild`
   * `AdditionalOptions`
   * `AdditionalIncludeDirectories`
   * `PreprocessorDefinitions`
   * `ForcedIncludeFiles`
   * `CompileAs`
10. Be fast enough for large solutions by evaluating projects in parallel and avoiding full builds.
11. Produce useful diagnostics when projects or files are skipped.
12. Be simple to use:

```bash
vscompilecommands MySolution.sln
```

and:

```bash
vscompilecommands MySolution.sln --configuration Debug --platform x64
```

---

## 3. Non-goals

The first version should not attempt to:

1. Build the project.
2. Replace CMake.
3. Replace MSBuild.
4. Generate compile commands for C#, Visual Basic, F#, or other managed-language files.
5. Fully emulate every exotic Visual C++ build customization.
6. Run custom MSBuild targets that mutate `ClCompile` items at execution time.
7. Generate compile commands for files that are not represented as evaluated `ClCompile` items.
8. Guarantee compatibility with MSBuild versions older than 17.8.
9. Generate a perfect command line for every third-party Visual Studio extension or custom build rule.
10. Modify `.sln`, `.vcxproj`, `.props`, or `.targets` files.
11. Require the project to be built first.

A future version may add fallback support for older MSBuild versions or advanced target-query modes, but the first version should remain evaluation-only.

---

## 4. Command-line interface

### Basic syntax

```bash
vscompilecommands <input> [options]
```

`<input>` must be either:

```text
MySolution.sln
MyProject.vcxproj
```

### Options

```bash
vscompilecommands MySolution.sln \
  --configuration Debug \
  --platform x64 \
  --output compile_commands.json \
  --msbuild-path "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" \
  --verbose
```

### CLI option design

| Option                   | Meaning                                                                                                           |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------- |
| `<input>`                | Required path to `.sln` or `.vcxproj`.                                                                            |
| `--configuration <name>` | Optional Visual Studio configuration, for example `Debug` or `Release`.                                           |
| `--platform <name>`      | Optional Visual Studio platform, for example `x64`, `Win32`, `ARM64`.                                             |
| `--output <path>`        | Optional output path. Defaults to `compile_commands.json` next to the input file.                                 |
| `--msbuild-path <path>`  | Optional explicit path to `MSBuild.exe`.                                                                          |
| `--jobs <n>`             | Optional project-evaluation parallelism. Defaults to a conservative CPU-based value, such as `min(cpu_count, 4)`. |
| `--verbose`              | Print skipped projects, selected configurations, and summary diagnostics.                                         |
| `--debug`                | Print MSBuild commands, raw failure details, JSON parse failures, and detailed metadata mapping diagnostics.      |
| `--dry-run`              | Discover and evaluate inputs, print what would be generated, but do not write the output file.                    |
| `--strict`               | Treat project evaluation failures as fatal. Without this, failed projects are skipped with warnings.              |

### Example invocations

Generate using the first available solution configuration/platform:

```bash
vscompilecommands build/MySolution.sln
```

Generate explicitly for `Debug|x64`:

```bash
vscompilecommands build/MySolution.sln --configuration Debug --platform x64
```

Generate from one project:

```bash
vscompilecommands src/MyProject/MyProject.vcxproj --configuration Release --platform x64
```

Write to a specific location:

```bash
vscompilecommands build/MySolution.sln --output build/compile_commands.json
```

Show selected projects without writing:

```bash
vscompilecommands build/MySolution.sln --configuration Debug --platform x64 --dry-run --verbose
```

---

## 5. Default behavior

### Output path

If `--output` is not supplied:

| Input type | Default output                               |
| ---------- | -------------------------------------------- |
| `.sln`     | `<solution-directory>/compile_commands.json` |
| `.vcxproj` | `<project-directory>/compile_commands.json`  |

This matches the common convention that `compile_commands.json` is placed at the top of the build directory. Clang’s compilation database documentation describes the convention of naming the file `compile_commands.json` and placing it at the top of the build directory. ([Clang][4])

### Configuration/platform defaults for `.sln`

When the input is a solution:

1. Parse solution configuration/platform entries in declaration order.
2. Prefer `GlobalSection(SolutionConfigurationPlatforms)` if present.
3. If absent, tolerate legacy `GlobalSection(SolutionConfiguration)`.
4. If neither `--configuration` nor `--platform` is supplied, choose the first solution configuration/platform pair.
5. If only `--configuration` is supplied, choose the first platform for that configuration.
6. If only `--platform` is supplied, choose the first configuration for that platform.
7. If the requested pair does not exist, fail with a message listing available pairs.

Example error:

```text
error: solution configuration 'Debug|x64' was not found in MySolution.sln.

Available solution configurations:
  Debug|Win32
  Release|Win32
  Release|x64
```

### Configuration/platform defaults for `.vcxproj`

When the input is a project:

1. Query MSBuild for `ProjectConfiguration` items.
2. Use the first evaluated `ProjectConfiguration` item when neither option is supplied.
3. Match requested `--configuration` and/or `--platform` against available pairs.
4. Use case-insensitive matching.
5. Treat `x86` as an alias for `Win32` only as a fallback.

The preferred query is:

```bash
msbuild MyProject.vcxproj ^
  -nologo ^
  -verbosity:quiet ^
  -tl:off ^
  -getItem:ProjectConfiguration
```

The expected item identities are values like:

```text
Debug|Win32
Debug|x64
Release|Win32
Release|x64
```

---

## 6. MSBuild integration

### Requirement

The tool should require MSBuild 17.8 or newer for the first version because the design depends on `-getItem` and `-getProperty` query options. Microsoft documents these options as available in MSBuild 17.8 and later. ([Microsoft Learn][3])

### Locating MSBuild

Resolution order:

1. `--msbuild-path`, if supplied.
2. `MSBUILD_EXE_PATH` environment variable, if set.
3. `MSBuild.exe` on `PATH`.
4. `vswhere.exe` lookup for installed Visual Studio instances.
5. Fail with a clear error.

Suggested `vswhere` command:

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest `
  -products * `
  -requires Microsoft.Component.MSBuild `
  -find MSBuild\**\Bin\MSBuild.exe
```

If the implementation is written in .NET and directly uses MSBuild APIs later, Microsoft recommends locating an appropriate MSBuild installation with `Microsoft.Build.Locator` when running on machines with varying Visual Studio, Build Tools, or .NET SDK installations. ([Microsoft Learn][5])

### Version check

Run:

```bash
msbuild -nologo -version
```

Parse the version. If it is older than 17.8:

```text
error: MSBuild 17.8 or newer is required for -getItem/-getProperty support.
found: C:\...\MSBuild.exe, version 17.6.3
```

### Evaluation command for one `.vcxproj`

For each selected project/configuration/platform pair:

```bash
msbuild MyProject.vcxproj ^
  -nologo ^
  -verbosity:quiet ^
  -tl:off ^
  -nr:false ^
  -p:Configuration=Debug ^
  -p:Platform=x64 ^
  -getProperty:MSBuildProjectDirectory,ProjectDir,IntDir,OutDir,TargetName,TargetExt,PlatformToolset,VCToolsInstallDir,VCToolsVersion,WindowsTargetPlatformVersion,IncludePath,ExternalIncludePath,CLToolExe,CLToolPath ^
  -getItem:ClCompile
```

Important details:

1. Do **not** pass `-target`, `-t`, or `-getTargetResult` in the default mode.
2. Do **not** build the project.
3. Disable terminal logger with `-tl:off` because MSBuild’s command-line reference warns not to parse terminal logger output. ([Microsoft Learn][6])
4. Use `-nologo` and quiet verbosity to keep stdout parseable.
5. Parse stdout as JSON.
6. Treat stderr as diagnostics only unless MSBuild exits nonzero.

MSBuild command-line properties are passed with `-p:name=value`; Microsoft documents `-property`/`-p` as setting or overriding project-level properties. ([Microsoft Learn][6])

### Expected JSON shape

The output should be parsed as a JSON object like:

```json
{
  "Properties": {
    "MSBuildProjectDirectory": "C:\\repo\\build\\MyProject",
    "ProjectDir": "C:\\repo\\build\\MyProject\\",
    "IntDir": "MyProject.dir\\Debug\\",
    "OutDir": "C:\\repo\\build\\Debug\\",
    "PlatformToolset": "v143"
  },
  "Items": {
    "ClCompile": [
      {
        "Identity": "..\\src\\main.cpp",
        "FullPath": "C:\\repo\\src\\main.cpp",
        "AdditionalIncludeDirectories": "C:\\repo\\include;%(AdditionalIncludeDirectories)",
        "PreprocessorDefinitions": "WIN32;_DEBUG;%(PreprocessorDefinitions)",
        "AdditionalOptions": "/bigobj %(AdditionalOptions)",
        "ExcludedFromBuild": "false"
      }
    ]
  }
}
```

The parser must tolerate absent properties or metadata. Missing optional metadata should be treated as empty.

### Failure handling

For one project evaluation failure:

* In default mode:

  * emit a warning,
  * skip the project,
  * continue.
* In `--strict` mode:

  * stop immediately,
  * return nonzero.

Example:

```text
warning: failed to evaluate ProjectA.vcxproj for Debug|x64.
reason: MSBuild exited with code 1.
use --debug to show full MSBuild stdout/stderr.
```

---

## 7. Solution parsing

A `.sln` file is text-based and contains project declarations plus global sections such as solution and project configuration mappings. Microsoft’s documentation shows `Project(...) = ...` blocks and `GlobalSection(...)` blocks as part of the solution file body. ([Microsoft Learn][7])

The tool should parse only the solution information it needs:

1. Project entries.
2. Project type GUID.
3. Project name.
4. Relative project path.
5. Project instance GUID.
6. `SolutionConfigurationPlatforms`.
7. `ProjectConfigurationPlatforms`.

### Project discovery

For each project block:

```text
Project("{TYPE-GUID}") = "Name", "relative\path\Project.vcxproj", "{PROJECT-GUID}"
EndProject
```

Rules:

1. Resolve project paths relative to the solution directory.
2. Include only files ending in `.vcxproj`, case-insensitive.
3. Skip solution folders.
4. Skip `.csproj`, `.vbproj`, `.fsproj`, `.sqlproj`, `.vdproj`, `.wixproj`, and unknown extensions.
5. Skip missing files with a warning in verbose mode.
6. Skip unloaded projects if they have no usable project path or no project configuration mapping.

### Solution configuration mapping

For the selected solution pair, for example:

```text
Debug|x64
```

read project mappings from:

```text
GlobalSection(ProjectConfigurationPlatforms) = postSolution
  {PROJECT-GUID}.Debug|x64.ActiveCfg = Debug|x64
  {PROJECT-GUID}.Debug|x64.Build.0 = Debug|x64
EndGlobalSection
```

Selection rules:

1. `ActiveCfg` gives the project configuration/platform to evaluate.
2. `Build.0` indicates the project participates in that solution build configuration.
3. By default, include only projects with `Build.0`.
4. If `ActiveCfg` exists but `Build.0` is absent, skip the project and report:

```text
skipped: ProjectA.vcxproj is not selected for build under Debug|x64.
```

Optional future flag:

```bash
--include-unbuilt-projects
```

could include projects with `ActiveCfg` but no `Build.0`, but that should not be part of the initial required feature set.

---

## 8. Project evaluation

Each `.vcxproj` is evaluated independently for a concrete project configuration/platform pair.

### Inputs

```text
project_path
configuration
platform
msbuild_path
```

### Output model

```text
EvaluatedProject
  project_path
  project_dir
  configuration
  platform
  properties
  cl_compile_items[]
  diagnostics[]
```

### Evaluation algorithm

1. Invoke MSBuild with `-getProperty` and `-getItem:ClCompile`.
2. Parse JSON.
3. Extract project-level properties.
4. Extract `Items.ClCompile`.
5. For each `ClCompile` item:

   * resolve `FullPath`,
   * read relevant metadata,
   * skip if excluded,
   * generate one compile command entry.

### Required metadata fields

The mapper should read at least:

```text
FullPath
Identity
ExcludedFromBuild
AdditionalIncludeDirectories
PreprocessorDefinitions
UndefinePreprocessorDefinitions
ForcedIncludeFiles
AdditionalOptions
CompileAs
LanguageStandard
LanguageStandard_C
PrecompiledHeader
PrecompiledHeaderFile
ObjectFileName
RuntimeLibrary
WarningLevel
TreatWarningAsError
Optimization
DebugInformationFormat
ExceptionHandling
RuntimeTypeInfo
EnableEnhancedInstructionSet
DisableSpecificWarnings
```

Unknown metadata should not cause failure.

---

## 9. Compile command generation

The output should use the JSON Compilation Database format: an array of command objects. Clang documents that each object specifies the working directory, the main translation unit file, and either `arguments` or `command`; `arguments` is preferred because shell escaping is a source of errors. ([Clang][4])

### Preferred output field

Use `arguments`, not `command`, by default.

Example:

```json
[
  {
    "directory": "C:\\repo\\build\\MyProject",
    "file": "C:\\repo\\src\\main.cpp",
    "arguments": [
      "cl.exe",
      "/nologo",
      "/TP",
      "/I", "C:\\repo\\include",
      "/DWIN32",
      "/D_DEBUG",
      "/std:c++20",
      "/c",
      "C:\\repo\\src\\main.cpp"
    ]
  }
]
```

### `directory`

Use the evaluated project directory:

1. Prefer `ProjectDir`.
2. Else use `MSBuildProjectDirectory`.
3. Else use the `.vcxproj` parent directory.

Normalize to an absolute path.

### `file`

Use `FullPath` from the evaluated `ClCompile` item.

Normalize to an absolute path.

### `arguments[0]`: compiler executable

Resolution order:

1. If `CLToolPath` and `CLToolExe` are available, combine them.
2. If `CLToolExe` is available, use it.
3. If `PlatformToolset` indicates ClangCL, use `clang-cl.exe`.
4. Otherwise use `cl.exe`.

The first version does not need to guarantee an absolute compiler path. Most compile database consumers care primarily about driver mode and arguments. However, using the evaluated compiler path when available is preferred.

### Required base arguments

Always include:

```text
/nologo
/c
<source-file>
```

The source file should normally be the final argument.

### Include directories

Read:

```text
AdditionalIncludeDirectories
ExternalIncludePath
IncludePath
```

For the first version:

1. Convert semicolon-delimited entries to individual paths.
2. Drop empty entries.
3. Drop unresolved inheritance markers such as:

   * `%(AdditionalIncludeDirectories)`
   * `%(ExternalIncludeDirectories)`
4. Resolve relative paths against `directory`.
5. Emit each include directory as:

```text
/I <path>
```

Using separate argv entries is preferred:

```json
["/I", "C:\\repo\\include"]
```

### Preprocessor definitions

Read:

```text
PreprocessorDefinitions
```

For each definition:

```text
WIN32
_DEBUG
FOO=1
NAME="value with spaces"
```

emit:

```text
/DWIN32
/D_DEBUG
/DFOO=1
/DNAME=value with spaces
```

Because `arguments` is an argv array, definitions with spaces do not need shell quoting.

Drop unresolved inheritance markers such as:

```text
%(PreprocessorDefinitions)
```

### Undefines

Read:

```text
UndefinePreprocessorDefinitions
```

Emit:

```text
/U<name>
```

Example:

```json
["/UDEBUG"]
```

### Language mode

Read:

```text
CompileAs
```

Mapping:

| Metadata value | Argument                  |
| -------------- | ------------------------- |
| `CompileAsC`   | `/TC`                     |
| `CompileAsCpp` | `/TP`                     |
| empty/default  | infer from file extension |

Extension inference:

| Extension                                             | Argument |
| ----------------------------------------------------- | -------- |
| `.c`                                                  | `/TC`    |
| `.cpp`, `.cc`, `.cxx`, `.c++`, `.ixx`, `.cppm`, `.mm` | `/TP`    |

Read:

```text
LanguageStandard
LanguageStandard_C
```

Suggested mapping:

| Metadata value | Argument         |
| -------------- | ---------------- |
| `stdcpp14`     | `/std:c++14`     |
| `stdcpp17`     | `/std:c++17`     |
| `stdcpp20`     | `/std:c++20`     |
| `stdcpplatest` | `/std:c++latest` |
| `stdc11`       | `/std:c11`       |
| `stdc17`       | `/std:c17`       |
| `stdclatest`   | `/std:clatest`   |

Ignore `Default`.

### Forced includes

Read:

```text
ForcedIncludeFiles
```

Emit:

```text
/FI<path>
```

or:

```json
["/FI", "C:\\repo\\include\\pch.h"]
```

Prefer the two-argument form only if the compiler accepts it consistently. Otherwise use one argument:

```json
["/FIC:\\repo\\include\\pch.h"]
```

### Precompiled headers

Read:

```text
PrecompiledHeader
PrecompiledHeaderFile
```

Mapping:

| Metadata value      | Argument        |
| ------------------- | --------------- |
| `Use`               | `/Yu<header>`   |
| `Create`            | `/Yc<header>`   |
| `NotUsing` or empty | no PCH argument |

If `PrecompiledHeaderFile` exists:

```text
/Fp<path>
```

For correctness, emit PCH flags by default. A future `--tooling-friendly` mode may omit PCH flags for tools that struggle with MSVC PCH options.

### Additional compiler options

Read:

```text
AdditionalOptions
```

Rules:

1. Remove inherited marker:

```text
%(AdditionalOptions)
```

2. Split using Windows command-line parsing rules.
3. Append near the end of the argument list, before `/c` and the source file when possible.
4. Preserve order.

This lets per-file custom options override earlier project-level options.

### Other common metadata mappings

The first version should support the following common options:

| Metadata                 |           Example value | Argument |
| ------------------------ | ----------------------: | -------- |
| `WarningLevel`           |                `Level3` | `/W3`    |
| `WarningLevel`           |                `Level4` | `/W4`    |
| `TreatWarningAsError`    |                  `true` | `/WX`    |
| `RuntimeLibrary`         |      `MultiThreadedDLL` | `/MD`    |
| `RuntimeLibrary`         | `MultiThreadedDebugDLL` | `/MDd`   |
| `RuntimeLibrary`         |         `MultiThreaded` | `/MT`    |
| `RuntimeLibrary`         |    `MultiThreadedDebug` | `/MTd`   |
| `Optimization`           |              `Disabled` | `/Od`    |
| `Optimization`           |              `MaxSpeed` | `/O2`    |
| `Optimization`           |              `MinSpace` | `/O1`    |
| `DebugInformationFormat` |       `ProgramDatabase` | `/Zi`    |
| `DebugInformationFormat` |              `OldStyle` | `/Z7`    |
| `ExceptionHandling`      |                  `Sync` | `/EHsc`  |
| `ExceptionHandling`      |                 `Async` | `/EHa`   |
| `RuntimeTypeInfo`        |                  `true` | `/GR`    |
| `RuntimeTypeInfo`        |                 `false` | `/GR-`   |

Unrecognized metadata should be ignored unless it appears in `AdditionalOptions`.

### Optional `output` field

The `output` field is optional in the compilation database format. ([Clang][4])

For the first version:

1. Include `output` only when `ObjectFileName` can be resolved confidently.
2. Otherwise omit it.

Example:

```json
{
  "directory": "C:\\repo\\build\\MyProject",
  "file": "C:\\repo\\src\\main.cpp",
  "output": "C:\\repo\\build\\MyProject\\Debug\\main.obj",
  "arguments": ["cl.exe", "/nologo", "/TP", "/c", "C:\\repo\\src\\main.cpp"]
}
```

---

## 10. Handling edge cases

### Files excluded from build

If a `ClCompile` item has:

```text
ExcludedFromBuild=true
```

for the selected configuration/platform, skip it.

Diagnostic in verbose mode:

```text
skipped file: src\old.cpp is excluded from build in Debug|x64.
```

### Per-file compiler options

Per-file metadata is already present on each evaluated `ClCompile` item. Generate each command from that item’s metadata, not from project-level assumptions.

Example:

```text
main.cpp:
  AdditionalOptions = /bigobj

legacy.cpp:
  AdditionalOptions = /wd4996
```

Each file gets its own argument list.

### Generated files

If a generated source file appears as an evaluated `ClCompile` item:

1. Include it if the path is concrete.
2. Warn if the file does not currently exist.
3. Do not fail by default.

Example:

```text
warning: generated source C:\repo\build\generated\foo.cpp does not exist yet; entry was still emitted.
```

### Relative paths

Normalize these to absolute paths:

1. `directory`
2. `file`
3. include directories
4. forced include files
5. optional `output`

Relative paths should be resolved against the project directory unless metadata clearly uses an absolute path.

### Spaces in paths

Use `arguments` rather than `command` to avoid shell quoting bugs.

Good:

```json
"arguments": [
  "cl.exe",
  "/I",
  "C:\\repo\\path with spaces\\include",
  "/c",
  "C:\\repo\\src\\main file.cpp"
]
```

Avoid generating this by default:

```json
"command": "cl.exe /I \"C:\\repo\\path with spaces\\include\" /c \"C:\\repo\\src\\main file.cpp\""
```

### Win32 vs x64

Visual C++ commonly uses `Win32` as the 32-bit platform name. The tool should:

1. Match requested platforms case-insensitively.
2. Treat `x86` as a fallback alias for `Win32`.
3. Never rewrite `x64`, `ARM`, or `ARM64`.
4. Print the actual selected project platform in verbose output.

Example:

```text
selected solution configuration: Debug|x64
selected project ProjectA.vcxproj configuration: Debug|Win32
```

This can happen because solution configuration mappings may map a solution platform to a different project platform.

### Missing MSBuild

Fail early:

```text
error: MSBuild.exe was not found.

Tried:
  --msbuild-path
  MSBUILD_EXE_PATH
  PATH
  Visual Studio Installer/vswhere lookup

Install Visual Studio Build Tools or pass --msbuild-path.
```

### Multiple installed Visual Studio versions

Default to the newest MSBuild that supports `-getItem`.

Verbose output:

```text
using MSBuild:
  C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
  version: 17.10.4
```

If the user passes `--msbuild-path`, use that exact executable.

### Projects that fail to evaluate

Default behavior:

```text
warning: skipped ProjectB.vcxproj because MSBuild evaluation failed.
```

`--strict` behavior:

```text
error: ProjectB.vcxproj failed to evaluate for Debug|x64.
```

### Duplicate source files across projects

Keep duplicate entries if the same file is compiled by multiple projects or configurations. The Clang specification allows multiple command objects for the same file when it is compiled in different ways. ([Clang][4])

Deduplicate only exact duplicates:

```text
same file
same directory
same arguments
same output
```

### Non-C/C++ projects in mixed solutions

Skip by extension:

```text
.csproj
.vbproj
.fsproj
.sqlproj
.wixproj
.vdproj
```

Verbose diagnostic:

```text
skipped project: App.csproj is not a .vcxproj project.
```

### Unsupported `.vcxproj` customization

If metadata contains unresolved macros after MSBuild evaluation, emit a warning in debug mode:

```text
debug: unresolved macro in include directory for ProjectA.vcxproj:
  $(SomeCustomMacro)\include
```

Default behavior should not fail unless the unresolved value prevents path construction for the source file itself.

---

## 11. Performance strategy

The tool must avoid full builds. MSBuild’s `-getItem` and `-getProperty` query mode can return evaluated data without running build targets when no target is specified. ([Microsoft Learn][3])

### Performance rules

1. Parse `.sln` once.
2. Evaluate each `.vcxproj` once per selected configuration/platform.
3. Do not invoke MSBuild per source file.
4. Evaluate projects in parallel.
5. Limit default parallelism to avoid spawning too many MSBuild processes.
6. Cache in memory by:

```text
project_path + configuration + platform + msbuild_path
```

7. Avoid disk caching in the first version.
8. Avoid binary logs unless `--debug` explicitly enables them in a future version.
9. Use quiet MSBuild output to reduce stdout/stderr volume.
10. Parse JSON directly.

### Suggested default parallelism

```text
jobs = min(logical_cpu_count, 4)
```

Allow override:

```bash
vscompilecommands MySolution.sln --jobs 8
```

### Large solution behavior

For a solution with 300 projects:

1. Discover all projects quickly from `.sln`.
2. Filter to `.vcxproj`.
3. Apply solution configuration mapping.
4. Evaluate selected `.vcxproj` projects with bounded parallelism.
5. Generate output incrementally in memory.
6. Write JSON once at the end.

---

## 12. Output format

The output file is a JSON array.

Default field set:

```json
[
  {
    "directory": "C:\\repo\\build\\ProjectA",
    "file": "C:\\repo\\src\\main.cpp",
    "arguments": [
      "cl.exe",
      "/nologo",
      "/TP",
      "/I",
      "C:\\repo\\include",
      "/DWIN32",
      "/D_DEBUG",
      "/std:c++20",
      "/c",
      "C:\\repo\\src\\main.cpp"
    ]
  }
]
```

Optional field:

```json
"output": "C:\\repo\\build\\ProjectA\\Debug\\main.obj"
```

### JSON formatting

Use stable pretty-printed JSON:

1. Two-space indentation.
2. Deterministic entry order:

   * solution project order,
   * then `ClCompile` item order from MSBuild.
3. UTF-8 encoding.
4. No BOM unless required by platform conventions; prefer no BOM.

### Ordering

For `.sln` inputs:

```text
project order from solution
file order from MSBuild ClCompile item list
```

For `.vcxproj` inputs:

```text
file order from MSBuild ClCompile item list
```

---

## 13. Error handling and diagnostics

### Exit codes

| Exit code | Meaning                                                    |
| --------: | ---------------------------------------------------------- |
|       `0` | Success. Output written. Warnings may exist.               |
|       `1` | Fatal input, CLI, MSBuild, or JSON parsing error.          |
|       `2` | No compile commands generated.                             |
|       `3` | One or more project evaluations failed in `--strict` mode. |

### Default output

Default mode should be quiet:

```text
wrote C:\repo\build\compile_commands.json
projects: 12 evaluated, 3 skipped
files: 842 compile commands
warnings: 1
```

### Verbose output

Verbose mode should include:

```text
input: C:\repo\build\MySolution.sln
selected solution configuration: Debug|x64
using MSBuild: C:\...\MSBuild.exe
evaluated: ProjectA.vcxproj Debug|x64 -> 120 files
skipped: App.csproj is not a .vcxproj project
skipped: ProjectB.vcxproj is not selected for build under Debug|x64
wrote: C:\repo\build\compile_commands.json
```

### Debug output

Debug mode should include:

1. Full MSBuild command lines.
2. MSBuild exit codes.
3. JSON parse errors.
4. Unresolved metadata.
5. Per-file skip reasons.
6. Compiler argument mapping decisions.

Example:

```text
debug: invoking:
  msbuild C:\repo\build\ProjectA\ProjectA.vcxproj -nologo -verbosity:quiet -tl:off -nr:false -p:Configuration=Debug -p:Platform=x64 -getProperty:... -getItem:ClCompile

debug: ProjectA.vcxproj ClCompile item:
  FullPath = C:\repo\src\main.cpp
  AdditionalIncludeDirectories = C:\repo\include;%(AdditionalIncludeDirectories)
  PreprocessorDefinitions = WIN32;_DEBUG;%(PreprocessorDefinitions)
```

### Error examples

Invalid input:

```text
error: input file does not exist: C:\repo\build\Missing.sln
```

Unsupported input:

```text
error: unsupported input extension '.txt'. Expected .sln or .vcxproj.
```

No C++ projects:

```text
error: no .vcxproj projects were found in MySolution.sln.
```

No compile files:

```text
error: no ClCompile items were found for Debug|x64.
```

Invalid configuration:

```text
error: configuration/platform 'RelWithDebInfo|x64' was not found.

Available:
  Debug|Win32
  Debug|x64
  Release|Win32
  Release|x64
```

---

## 14. Testing strategy

### Unit tests

#### CLI parsing

Test:

```text
vscompilecommands MySolution.sln
vscompilecommands MySolution.sln --configuration Debug
vscompilecommands MySolution.sln --platform x64
vscompilecommands MySolution.sln --configuration Debug --platform x64
vscompilecommands MySolution.sln --output out.json
vscompilecommands MySolution.sln --dry-run
```

Expected:

1. Correct parsed options.
2. Missing input fails.
3. Unknown option fails.
4. Unsupported extension fails.

#### Solution parser

Fixtures:

1. Single `.vcxproj`.
2. Mixed `.sln` with `.csproj` and `.vcxproj`.
3. Solution folders.
4. Missing project file.
5. Multiple solution configurations.
6. Project mapped to different platform than the solution.
7. Project present but not selected for build.

Assertions:

1. `.vcxproj` projects discovered.
2. Non-C++ projects skipped.
3. Selected solution configuration resolved.
4. Project `ActiveCfg` and `Build.0` handled correctly.

#### MSBuild JSON parser

Use fixture JSON rather than invoking MSBuild.

Test:

1. One `ClCompile` item.
2. Multiple `ClCompile` items.
3. Missing `Items`.
4. Missing `ClCompile`.
5. Missing optional metadata.
6. Malformed JSON.

#### Metadata splitting

Test semicolon-list parsing:

```text
C:\include;..\relative;%(AdditionalIncludeDirectories)
```

Expected:

```text
C:\include
<absolute path to relative>
```

and inherited marker removed.

Test definitions:

```text
WIN32;_DEBUG;FOO=1;NAME=value with spaces;%(PreprocessorDefinitions)
```

Expected:

```text
/DWIN32
/D_DEBUG
/DFOO=1
/DNAME=value with spaces
```

#### Argument generation

Test mappings for:

1. Include directories.
2. Defines.
3. Undefines.
4. Forced includes.
5. Language standard.
6. Compile-as C.
7. Compile-as C++.
8. Runtime library.
9. Warning level.
10. Per-file `AdditionalOptions`.

### Integration tests

Run only when MSBuild 17.8+ is available.

#### Single `.vcxproj`

Fixture:

```text
ProjectA.vcxproj
  src/main.cpp
  include/projecta.h
```

Expected:

1. Output file exists.
2. One compile command exists.
3. `file` points to `main.cpp`.
4. include path emitted.
5. define emitted.

#### Full `.sln`

Fixture:

```text
MySolution.sln
  ProjectA.vcxproj
  ProjectB.vcxproj
```

Expected:

1. Both projects evaluated.
2. Entries from both projects emitted.
3. Order follows solution order.

#### Mixed C#/C++ solution

Fixture:

```text
MySolution.sln
  NativeLib.vcxproj
  ManagedApp.csproj
```

Expected:

1. Native project included.
2. C# project skipped.
3. No C# files emitted.

#### Debug/x64 and Release/x64

Same project with different metadata:

```text
Debug|x64:
  _DEBUG
  /Od

Release|x64:
  NDEBUG
  /O2
```

Expected:

1. Debug output contains `_DEBUG`.
2. Release output contains `NDEBUG`.
3. Arguments differ.

#### Custom include paths and defines

Project metadata:

```xml
AdditionalIncludeDirectories = include;third_party\lib\include
PreprocessorDefinitions = USE_FEATURE;VALUE=42
```

Expected:

```text
/I <project>\include
/I <project>\third_party\lib\include
/DUSE_FEATURE
/DVALUE=42
```

#### Per-file compiler options

One file has:

```text
AdditionalOptions = /bigobj
```

Expected only that file’s entry contains `/bigobj`.

#### Paths containing spaces

Fixture directory:

```text
C:\test fixtures\project with spaces\
```

Expected:

1. `arguments` array preserves spaces without shell quoting.
2. JSON parses correctly.
3. No broken command string.

#### Missing or invalid configuration/platform

Invoke:

```bash
vscompilecommands MySolution.sln --configuration Missing --platform x64
```

Expected:

1. Nonzero exit.
2. Available configurations printed.

#### Excluded files

One file:

```text
ExcludedFromBuild=true
```

Expected:

1. File omitted.
2. Verbose mode reports skip reason.

#### Duplicate source file

Two projects compile the same source file with different defines.

Expected:

1. Two entries remain.
2. Each entry has project-specific arguments.

---

## 15. Suggested implementation plan

### Milestone 1: CLI skeleton

Implement:

1. Argument parser.
2. Input validation.
3. Output path defaulting.
4. Logging levels:

   * normal,
   * verbose,
   * debug.
5. Dry-run plumbing.
6. Exit code handling.

Deliverable:

```bash
vscompilecommands MySolution.sln --help
```

works and validates inputs.

---

### Milestone 2: Locate MSBuild

Implement:

1. `--msbuild-path`.
2. `MSBUILD_EXE_PATH`.
3. `PATH` lookup.
4. `vswhere` lookup.
5. Version check.
6. Friendly error messages.

Deliverable:

```bash
vscompilecommands MyProject.vcxproj --debug
```

prints the selected MSBuild path and version.

---

### Milestone 3: Parse `.sln`

Implement:

1. Project block parser.
2. Solution configuration parser.
3. Project configuration mapping parser.
4. `.vcxproj` filtering.
5. C# and other project skipping.
6. Default solution configuration selection.

Deliverable:

```bash
vscompilecommands MySolution.sln --dry-run --verbose
```

prints selected `.vcxproj` projects and project configuration/platform pairs.

---

### Milestone 4: Query project configurations for `.vcxproj`

Implement:

1. `msbuild -getItem:ProjectConfiguration`.
2. JSON parser.
3. Default project configuration/platform selection.
4. Invalid configuration diagnostics.

Deliverable:

```bash
vscompilecommands MyProject.vcxproj --dry-run --verbose
```

prints selected `Debug|x64` or first available pair.

---

### Milestone 5: Query `ClCompile` items

Implement:

1. MSBuild evaluation command.
2. `-getProperty` list.
3. `-getItem:ClCompile`.
4. JSON parsing.
5. Error handling.
6. Project evaluation result model.

Deliverable:

```bash
vscompilecommands MyProject.vcxproj --debug
```

prints number of evaluated `ClCompile` items.

---

### Milestone 6: Generate minimal compile commands

Generate entries with:

1. `directory`
2. `file`
3. `arguments`
4. compiler executable
5. `/nologo`
6. `/TC` or `/TP`
7. `/c`
8. source file

Deliverable:

```json
[
  {
    "directory": "C:\\repo\\build\\ProjectA",
    "file": "C:\\repo\\src\\main.cpp",
    "arguments": ["cl.exe", "/nologo", "/TP", "/c", "C:\\repo\\src\\main.cpp"]
  }
]
```

---

### Milestone 7: Add metadata mapping

Add support for:

1. Include directories.
2. Preprocessor definitions.
3. Undefines.
4. Forced includes.
5. Additional options.
6. Language standards.
7. Warning level.
8. Runtime library.
9. Optimization.
10. Debug information.
11. Exception handling.
12. Precompiled headers.

Deliverable:

Generated commands are useful for clangd/static analyzers on normal Visual C++ projects.

---

### Milestone 8: Write `compile_commands.json`

Implement:

1. Stable ordering.
2. Pretty JSON.
3. UTF-8 output.
4. Atomic write:

   * write temp file,
   * replace destination.
5. Summary output.

Deliverable:

```bash
vscompilecommands MySolution.sln --configuration Debug --platform x64
```

writes `compile_commands.json`.

---

### Milestone 9: Diagnostics

Implement:

1. Skipped project reasons.
2. Skipped file reasons.
3. MSBuild failure summaries.
4. Debug raw MSBuild command.
5. Unresolved macro warnings.
6. Final statistics.

Deliverable:

```text
wrote C:\repo\build\compile_commands.json
projects: 12 evaluated, 4 skipped
files: 842 compile commands
warnings: 2
```

---

### Milestone 10: Tests

Implement:

1. Unit tests for parsing.
2. Unit tests for metadata mapping.
3. Fixture-based JSON parser tests.
4. Integration tests gated on MSBuild availability.
5. Golden-file tests for output JSON.
6. CI job on Windows with Visual Studio Build Tools.

---

## 16. Recommended internal data model

```text
CliOptions
  input_path
  configuration?
  platform?
  output_path?
  msbuild_path?
  jobs
  verbose
  debug
  dry_run
  strict

SolutionInfo
  path
  directory
  configurations[]
  projects[]

SolutionProject
  name
  path
  extension
  project_guid
  type_guid

ProjectSelection
  project_path
  solution_configuration
  solution_platform
  project_configuration
  project_platform
  selected_for_build
  skip_reason?

EvaluatedProject
  project_path
  project_directory
  configuration
  platform
  properties
  cl_compile_items[]
  diagnostics[]

ClCompileItem
  identity
  full_path
  metadata

CompileCommand
  directory
  file
  arguments[]
  output?

Diagnostic
  level
  code
  message
  project_path?
  file_path?
```

---

## 17. End-to-end algorithm

```text
main(args):
  options = parse_cli(args)
  validate_input(options.input_path)

  msbuild = locate_msbuild(options.msbuild_path)
  validate_msbuild_version(msbuild, minimum=17.8)

  if input is .sln:
      solution = parse_solution(input)
      solution_pair = select_solution_configuration(solution, options)
      selections = select_vcxproj_projects(solution, solution_pair)
  else if input is .vcxproj:
      project_pairs = query_project_configurations(msbuild, input)
      project_pair = select_project_configuration(project_pairs, options)
      selections = [ProjectSelection(input, project_pair)]

  if selections is empty:
      fail("no .vcxproj projects selected")

  evaluated_projects = evaluate_projects_in_parallel(msbuild, selections, options.jobs)

  commands = []
  for project in evaluated_projects:
      for item in project.cl_compile_items:
          if is_excluded(item):
              record_skip()
              continue
          command = build_compile_command(project, item)
          if command is valid:
              commands.append(command)

  commands = deduplicate_exact_duplicates(commands)

  if commands is empty:
      exit 2

  if options.dry_run:
      print_summary(commands)
      exit 0

  write_json_atomic(options.output_path, commands)
  print_summary(commands)
  exit 0
```

---

## 18. First-version acceptance criteria

The first working version is complete when:

1. `vscompilecommands MyProject.vcxproj` writes a valid `compile_commands.json`.
2. `vscompilecommands MySolution.sln` discovers `.vcxproj` projects and ignores `.csproj`.
3. `--configuration` and `--platform` select the requested build configuration.
4. Missing configuration/platform reports available alternatives.
5. `ClCompile` items are retrieved through MSBuild, not project XML parsing.
6. Files with `ExcludedFromBuild=true` are skipped.
7. Per-file include paths, defines, forced includes, and additional options are reflected.
8. Paths with spaces work because output uses `arguments`.
9. Failed project evaluation does not crash the whole solution unless `--strict` is set.
10. Large solutions are evaluated with bounded parallelism.
11. Output is stable, pretty-printed JSON.
12. The tool never builds the project.

[1]: https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html "CMAKE_EXPORT_COMPILE_COMMANDS — CMake 4.3.2 Documentation"
[2]: https://learn.microsoft.com/en-us/cpp/build/reference/vcxproj-file-structure?view=msvc-170 ".vcxproj and .props file structure | Microsoft Learn"
[3]: https://learn.microsoft.com/en-us/visualstudio/msbuild/evaluate-items-and-properties?view=visualstudio "Evaluate MSBuild items and properties - MSBuild | Microsoft Learn"
[4]: https://clang.llvm.org/docs/JSONCompilationDatabase.html "JSON Compilation Database Format Specification — Clang 23.0.0git documentation"
[5]: https://learn.microsoft.com/en-us/visualstudio/msbuild/find-and-use-msbuild-versions?view=visualstudio "Find MSBuild and use its API - MSBuild | Microsoft Learn"
[6]: https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference?view=visualstudio "MSBuild Command-Line Reference - MSBuild | Microsoft Learn"
[7]: https://learn.microsoft.com/en-us/visualstudio/extensibility/internals/solution-dot-sln-file?view=visualstudio "Project Solution (.sln) file - Visual Studio (Windows) | Microsoft Learn"
