#include <fstream>
#include <iomanip>
#include <stdlib.h>
#include <unordered_set>
#include <vector>
#include <string>
#include <iostream>

#include <swift/Basic/LLVMInitialize.h>
#include <swift/FrontendTool/FrontendTool.h>

// TODO: move elsewhere with patchFrontendOptions
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <swift/Basic/OutputFileMap.h>
#include <unistd.h>
#include <swift/Basic/Platform.h>

#include "SwiftExtractor.h"

using namespace std::string_literals;

// This is part of the swiftFrontendTool interface, we hook into the
// compilation pipeline and extract files after the Swift frontend performed
// semantic analysys
class Observer : public swift::FrontendObserver {
 public:
  explicit Observer(const codeql::SwiftExtractorConfiguration& config) : config{config} {}

  void parsedArgs(swift::CompilerInvocation& invocation) override {
    auto& overlays = invocation.getSearchPathOptions().VFSOverlayFiles;

    /* overlays.push_back("/opt/GitHub/semmle-code/ql/swift/integration-tests/partial-modules/vfs.yaml");
     */

    std::cerr << "SETUP VFS: " << std::to_string(getpid()) << "\n";

    auto vfsPath = config.scratchDir + "/vfs/";
    if (llvm::sys::fs::exists(vfsPath)) {
      std::error_code ec;
      llvm::sys::fs::directory_iterator it(vfsPath, ec);
      // std::cerr << "VFS dir it: " << it->path() << ": " << ec.message() << "\n";
      while (!ec && it != llvm::sys::fs::directory_iterator()) {
        llvm::StringRef path(it->path());
        if (path.endswith("vfs.yaml")) {
          std::cerr << "VFS ENTRY: " << path.str() << "\n";
          invocation.getSearchPathOptions().VFSOverlayFiles.push_back(path.str());
        }
        it.increment(ec);
      }
    }
  }

  /* void configuredCompiler(swift::CompilerInstance& compiler) override { */
  /*   outputs =
   * compiler.getInvocation().getFrontendOptions().InputsAndOutputs.copyOutputFilenames(); */
  /* } */

  void performedSemanticAnalysis(swift::CompilerInstance& compiler) override {
    codeql::extractSwiftFiles(config, compiler);
  }

  /* std::vector<std::string> outputs; */

 private:
  const codeql::SwiftExtractorConfiguration& config;
};

static std::string getenv_or(const char* envvar, const std::string& def) {
  if (const char* var = getenv(envvar)) {
    return var;
  }
  return def;
}

static std::vector<std::string> getAliases(llvm::StringRef modulePath,
                                           const std::string& targetTriple) {
  if (modulePath.empty()) {
    return {};
  }

  llvm::SmallVector<llvm::StringRef> chunks;
  modulePath.split(chunks, '/');
  size_t intermediatesDirIndex = 0;
  for (size_t i = 0; i < chunks.size(); i++) {
    if (chunks[i] == "Intermediates.noindex") {
      intermediatesDirIndex = i;
      break;
    }
  }
  // Not built by Xcode, skipping
  if (intermediatesDirIndex == 0) {
    return {};
  }
  // e.g. Debug-iphoneos, Release-iphonesimulator, etc.
  auto destinationDir = chunks[intermediatesDirIndex + 2].str();
  auto arch = chunks[intermediatesDirIndex + 5].str();
  auto moduleNameWithExt = chunks.back();
  auto moduleName = moduleNameWithExt.substr(0, moduleNameWithExt.find_last_of('.'));
  std::string relocatedModulePath;
  for (size_t i = 0; i < intermediatesDirIndex; i++) {
    relocatedModulePath += '/' + chunks[i].str();
  }
  relocatedModulePath += "/Products/";
  relocatedModulePath += destinationDir + '/';

  std::vector<std::string> moduleLocations;

  std::string firstPath = relocatedModulePath;
  firstPath += moduleNameWithExt.str() + '/';
  moduleLocations.push_back(firstPath);

  std::string secondPath = relocatedModulePath;
  secondPath += '/' + moduleName.str() + '/';
  secondPath += moduleNameWithExt.str() + '/';
  moduleLocations.push_back(secondPath);

  std::string thirdPath = relocatedModulePath;
  thirdPath += '/' + moduleName.str() + '/';
  thirdPath += moduleName.str() + ".framework/Modules/";
  thirdPath += moduleNameWithExt.str() + '/';
  moduleLocations.push_back(thirdPath);

  std::vector<std::string> aliases;
  for (auto& location : moduleLocations) {
    aliases.push_back(location + arch + ".swiftmodule");
    if (!targetTriple.empty()) {
      llvm::Triple triple(targetTriple);
      auto moduleTriple = swift::getTargetSpecificModuleTriple(triple);
      aliases.push_back(location + moduleTriple.normalize() + ".swiftmodule");
    }
  }

  return aliases;
}

// TODO: move elsewhere
static void patchFrontendOptions(codeql::SwiftExtractorConfiguration& config,
                                 std::vector<std::string>& frontendOptions) {
  std::unordered_map<std::string, std::string> remapping;

  // TODO: handle filelists?
  // TODO: handle -Xcc arguments (e.g. -Xcc -Isomething)?
  std::unordered_set<std::string> pathRewriteOptions({
      "-emit-dependencies-path",
      "-emit-module-path",
      "-emit-module-doc-path",
      "-emit-module-source-info-path",
      "-emit-objc-header-path",
      "-emit-reference-dependencies-path",
      "-index-store-path",
      "-module-cache-path",
      "-o",
      "-pch-output-dir",
      "-serialize-diagnostics-path",
  });

  std::unordered_set<std::string> skipOne({"-Xcc"});

  std::unordered_set<std::string> searchPathRewriteOptions({"-I", "-F"});

  // Defaulting to then root assuming that all the paths are absolute
  // TODO: be more flexible and check if the path is actually an absolute one
  auto pathRewritePrefix = config.scratchDir + "/swift-extraction-artifacts";

  std::vector<size_t> searchPathIndexes;
  /* size_t supplementaryOutputsMapIndex = 0; */
  std::vector<size_t> outputFileMapIndexes;
  std::vector<std::string> maybeInput;

  std::string targetTriple;

  std::vector<std::string> newLocations;
  for (size_t i = 0; i < frontendOptions.size(); i++) {
    if (pathRewriteOptions.count(frontendOptions[i])) {
      auto oldPath = frontendOptions[i + 1];
      auto newPath = pathRewritePrefix + '/' + oldPath;
      frontendOptions[++i] = newPath;
      newLocations.push_back(newPath);

      remapping[oldPath] = newPath;
    } else if (frontendOptions[i] == "-supplementary-output-file-map" ||
               frontendOptions[i] == "-output-file-map") {
      // collect output map indexes for further rewriting and skip the following argument
      outputFileMapIndexes.push_back(++i);
    } else if (searchPathRewriteOptions.count(frontendOptions[i])) {
      // Collect search path options indexes for further new search path insertion
      searchPathIndexes.push_back(i);
      // Skip the following argument
      i++;
    } else if (frontendOptions[i] == "-target") {
      targetTriple = frontendOptions[++i];
    } else if (skipOne.count(frontendOptions[i])) {
      i++;
    } else if (frontendOptions[i][0] != '-') {
      // Maybe it's an input file generated by a previous invocation
      // In which case attempt to rewrite the argument if the new path exists
      auto oldPath = frontendOptions[i];
      maybeInput.push_back(oldPath);
      auto newPath = pathRewritePrefix + '/' + oldPath;
      if (llvm::sys::fs::exists(newPath)) {
        frontendOptions[i] = newPath;
      }
    }
  }

  /* for (auto index : outputFileMapIndexes) { */
  /*   auto oldPath = frontendOptions[index]; */
  /*   auto newPath = pathRewritePrefix + '/' + oldPath; */
  /*   frontendOptions[index] = newPath; */

  /*   llvm::errs() << "DEBUGG: map" << newPath << "\n"; */

  /*   // TODO: do not assume absolute path */
  /*   auto outputMapOrError = swift::OutputFileMap::loadFromPath(oldPath, ""); */
  /*   if (outputMapOrError) { */
  /*     auto oldOutputMap = outputMapOrError.get(); */
  /*     swift::OutputFileMap newOutputMap; */
  /*     std::vector<llvm::StringRef> keys; */
  /*     llvm::errs() << "DEBUGG: got map" */
  /*                  << "\n"; */
  /*     for (auto& key : maybeInput) { */
  /*       llvm::errs() << "DEBUGG: map key " << key << "\n"; */
  /*       auto oldMap = oldOutputMap.getOutputMapForInput(key); */
  /*       if (oldMap) { */
  /*         keys.push_back(key); */
  /*         auto& newMap = newOutputMap.getOrCreateOutputMapForInput(key); */
  /*         newMap.copyFrom(*oldMap); */
  /*         for (auto& entry : newMap) { */
  /*           auto oldPath = entry.getSecond(); */
  /*           auto newPath = pathRewritePrefix + '/' + oldPath; */
  /*           entry.getSecond() = newPath; */
  /*           llvm::errs() << "DEBUGG: rewritten entry " << entry.second << "\n"; */
  /*           remapping[oldPath] = newPath; */
  /*         } */
  /*       } */
  /*     } */
  /*     std::error_code ec; */
  /*     llvm::SmallString<PATH_MAX> filepath(newPath); */
  /*     llvm::StringRef parent = llvm::sys::path::parent_path(filepath); */
  /*     if (std::error_code ec = llvm::sys::fs::create_directories(parent)) { */
  /*       std::cerr << "Cannot create relocated output map dir: " << ec.message() << "\n"; */
  /*       return; */
  /*     } */

  /*     llvm::raw_fd_ostream fd(newPath, ec, llvm::sys::fs::OF_None); */
  /*     newOutputMap.write(fd, keys); */
  /*   } */
  /* } */

  // Re-create the directories as Swift frontend expects them to be present
  for (auto& location : newLocations) {
    llvm::SmallString<PATH_MAX> filepath(location);
    llvm::StringRef parent = llvm::sys::path::parent_path(filepath);
    if (std::error_code ec = llvm::sys::fs::create_directories(parent)) {
      std::cerr << "Cannot create patched directory: " << ec.message() << "\n";
      return;
    }
  }

  /* for (auto& [_, newPath] : remapping) { */
  /*   llvm::StringRef path(newPath); */
  /*   if (path.endswith(".swiftmodule")) { */
  /*     auto aliases = getAliases(path, targetTriple); */
  /*     for (auto& alias : aliases) { */
  /*       remapping[alias] = newPath; */
  /*     } */
  /*   } */
  /* } */

  auto vfsDir = config.scratchDir + "/vfs";
  {
    llvm::StringRef path(vfsDir);
    if (std::error_code ec = llvm::sys::fs::create_directories(path)) {
      std::cerr << "Cannot create VFS directory: " << ec.message() << "\n";
      return;
    }
  }

  std::unordered_map<std::string, std::string> modules;
  for (auto& [oldPath, newPath] : remapping) {
    if (llvm::StringRef(oldPath).endswith(".swiftmodule")) {
      modules[oldPath] = newPath;
    }
  }

  if (!modules.empty()) {
    auto vfsPath = vfsDir + '/' + std::to_string(getpid()) + "-vfs.yaml";
    std::error_code ec;
    llvm::raw_fd_ostream fd(vfsPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
      std::cerr << "Cannot create VFS file: '" << vfsPath << "': " << ec.message() << "\n";
      return;
    }
    fd << "{ version: 0,\n";
    /* fd << "  use-external-names: false,\n"; */
    fd << "  fallthrough: false,\n";
    fd << "  roots: [\n";
    for (auto& [oldPath, newPath] : modules) {
      fd << "  {\n";
      fd << "    type: 'file',\n";
      fd << "    name: '" << oldPath << "\',\n";
      fd << "    external-contents: '" << newPath << "\'\n";
      fd << "  },\n";
    }
    fd << "  ]\n";
    fd << "}\n";
    fd.flush();
  }

  /*   return; */

  /*   std::reverse(std::begin(searchPathIndexes), std::end(searchPathIndexes)); */
  /*   for (auto index : searchPathIndexes) { */
  /*     // Inserting new search path right before the existing ones */
  /*     auto option = frontendOptions[index]; */
  /*     auto oldSearchPath = frontendOptions[index + 1]; */
  /*     auto newSearchPath = pathRewritePrefix + '/' + oldSearchPath; */
  /*     if (llvm::sys::fs::exists(newSearchPath)) { */
  /*       auto it = frontendOptions.begin(); */
  /*       std::advance(it, index); */
  /*       it = frontendOptions.insert(it, newSearchPath); */
  /*       frontendOptions.insert(it, option); */
  /*     } */
  /*   } */
}

int main(int argc, char** argv) {
  if (argc == 1) {
    // TODO: print usage
    return 1;
  }
  // Required by Swift/LLVM
  PROGRAM_START(argc, argv);
  INITIALIZE_LLVM();

  codeql::SwiftExtractorConfiguration configuration{};
  configuration.trapDir = getenv_or("CODEQL_EXTRACTOR_SWIFT_TRAP_DIR", ".");
  configuration.sourceArchiveDir = getenv_or("CODEQL_EXTRACTOR_SWIFT_SOURCE_ARCHIVE_DIR", ".");
  configuration.scratchDir = getenv_or("CODEQL_EXTRACTOR_SWIFT_SCRATCH_DIR", ".");

  configuration.frontendOptions.reserve(argc - 1);
  for (int i = 1; i < argc; i++) {
    configuration.frontendOptions.push_back(argv[i]);
  }
  configuration.patchedFrontendOptions = configuration.frontendOptions;
  patchFrontendOptions(configuration, configuration.patchedFrontendOptions);

  std::vector<const char*> args;
  std::cerr << "DEBUG swift-frontend-args:\n";
  for (auto& arg : configuration.patchedFrontendOptions) {
    args.push_back(arg.c_str());
    std::cerr << "  " << std::quoted(arg) << " \\\n";
  }
  std::cerr << "\n\n";

  Observer observer(configuration);
  int frontend_rc = swift::performFrontend(args, "swift-extractor", (void*)main, &observer);

  /* if (!frontend_rc) { */
  /*   attemptToCopyMergedModule(observer.outputs); */
  /* } */

  //-supplementary-output-file-map
  /// var/folders/10/hts02tt52j1b7x26bym0bp0c0000gn/T/TemporaryDirectory.860KhC/supplementaryOutputs-1

  for (size_t i = 0; i < configuration.frontendOptions.size(); i++) {
    if (configuration.patchedFrontendOptions[i] == "-supplementary-output-file-map") {
      auto mapFile = configuration.frontendOptions[i + 1];
      auto filepath = getenv_or("CODEQL_EXTRACTOR_SWIFT_SCRATCH_DIR", "/tmp") + "/maps/" + mapFile;
      llvm::StringRef parent = llvm::sys::path::parent_path(filepath);
      if (std::error_code ec = llvm::sys::fs::create_directories(parent)) {
        std::cerr << "Cannot create relocated map directory: " << ec.message() << "\n";
        return frontend_rc;
      }
      llvm::SmallString<PATH_MAX> srcFilePath(mapFile);
      llvm::SmallString<PATH_MAX> dstFilePath(filepath);

      if (std::error_code ec = llvm::sys::fs::copy_file(srcFilePath, dstFilePath)) {
        std::cerr << "Cannot relocate map file '" << srcFilePath.str().str() << "' -> '"
                  << dstFilePath.str().str() << "': " << ec.message() << "\n";
      }
    }
  }

  return frontend_rc;
}
