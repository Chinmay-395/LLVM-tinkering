#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace llvm;

namespace {

using PointsToSet = std::set<const Value *>;
using PointsToMap = std::map<const Value *, PointsToSet>;

// valuePts[V] records the abstract memory locations to which pointer SSA value
// V may point. memoryPts[L] records the pointer values that may be stored in
// abstract memory location L. In this baseline, allocas are the only locations.
struct State {
  PointsToMap valuePts;
  PointsToMap memoryPts;

  bool operator==(const State &Other) const {
    return valuePts == Other.valuePts && memoryPts == Other.memoryPts;
  }

  bool operator!=(const State &Other) const { return !(*this == Other); }
};

static void unionSet(PointsToSet &Destination, const PointsToSet &Source) {
  Destination.insert(Source.begin(), Source.end());
}

static void unionMap(PointsToMap &Destination, const PointsToMap &Source) {
  for (const auto &Entry : Source)
    unionSet(Destination[Entry.first], Entry.second);
}

static void unionState(State &Destination, const State &Source) {
  unionMap(Destination.valuePts, Source.valuePts);
  unionMap(Destination.memoryPts, Source.memoryPts);
}

static PointsToSet getPointsTo(const Value *V, const State &S) {
  // An alloca instruction denotes the address of its own abstract object.
  if (isa<AllocaInst>(V))
    return {V};

  auto It = S.valuePts.find(V);
  return It == S.valuePts.end() ? PointsToSet{} : It->second;
}

static void transferInstruction(Instruction &I, State &S) {
  if (auto *AI = dyn_cast<AllocaInst>(&I)) {
    S.valuePts[AI].insert(AI);
    return;
  }

  if (auto *LI = dyn_cast<LoadInst>(&I)) {
    // Only a load whose result is a pointer contributes a points-to fact.
    if (!LI->getType()->isPointerTy())
      return;

    PointsToSet Result;
    for (const Value *Location : getPointsTo(LI->getPointerOperand(), S)) {
      auto It = S.memoryPts.find(Location);
      if (It != S.memoryPts.end())
        unionSet(Result, It->second);
    }

    S.valuePts[LI] = std::move(Result);
    return;
  }

  if (auto *SI = dyn_cast<StoreInst>(&I)) {
    const Value *Source = SI->getValueOperand();
    if (!Source->getType()->isPointerTy())
      return;

    const PointsToSet SourceSet = getPointsTo(Source, S);
    const PointsToSet DestinationSet =
        getPointsTo(SI->getPointerOperand(), S);

    if (DestinationSet.size() == 1) {
      // Exactly one target means this store kills the location's old fact.
      S.memoryPts[*DestinationSet.begin()] = SourceSet;
      return;
    }

    // With zero targets there is nothing known to update. With multiple
    // targets, retain every old fact because any one target may be modified.
    for (const Value *Location : DestinationSet)
      unionSet(S.memoryPts[Location], SourceSet);
  }
}

static std::string valueLabel(const Value *V) {
  std::string Buffer;
  raw_string_ostream OS(Buffer);
  V->printAsOperand(OS, false);
  OS.flush();
  return Buffer;
}

static std::vector<std::string> sortedLabels(const PointsToSet &Set) {
  std::vector<std::string> Labels;
  Labels.reserve(Set.size());
  for (const Value *V : Set)
    Labels.push_back(valueLabel(V));
  llvm::sort(Labels);
  return Labels;
}

static void printMap(StringRef Heading, const PointsToMap &Map) {
  errs() << "  " << Heading << ":\n";

  std::vector<std::pair<std::string, std::vector<std::string>>> Entries;
  Entries.reserve(Map.size());
  for (const auto &Entry : Map)
    Entries.emplace_back(valueLabel(Entry.first), sortedLabels(Entry.second));
  llvm::sort(Entries, [](const auto &Left, const auto &Right) {
    return Left.first < Right.first;
  });

  if (Entries.empty()) {
    errs() << "    <empty>\n";
    return;
  }

  for (const auto &Entry : Entries) {
    errs() << "    " << Entry.first << " -> { ";
    for (size_t I = 0; I < Entry.second.size(); ++I) {
      if (I != 0)
        errs() << ", ";
      errs() << Entry.second[I];
    }
    errs() << " }\n";
  }
}

static void printState(const BasicBlock &B, const State &S) {
  errs() << "BasicBlock: ";
  if (B.hasName())
    errs() << B.getName();
  else
    errs() << "<unnamed>";
  errs() << "\n";
  printMap("valuePts", S.valuePts);
  printMap("memoryPts", S.memoryPts);
}

class SimplePointsToPass : public PassInfoMixin<SimplePointsToPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    std::map<const BasicBlock *, State> Out;
    std::deque<BasicBlock *> Worklist;
    std::set<const BasicBlock *> InWorklist;

    // Starting with every block also gives deterministic output for unreachable
    // blocks. Successors are revisited whenever a predecessor changes.
    for (BasicBlock &B : F) {
      Worklist.push_back(&B);
      InWorklist.insert(&B);
    }

    while (!Worklist.empty()) {
      BasicBlock *B = Worklist.front();
      Worklist.pop_front();
      InWorklist.erase(B);

      State Current;
      for (const BasicBlock *Pred : predecessors(B))
        unionState(Current, Out[Pred]);

      for (Instruction &I : *B)
        transferInstruction(I, Current);

      if (Current == Out[B])
        continue;

      Out[B] = std::move(Current);
      for (BasicBlock *Successor : successors(B)) {
        if (InWorklist.insert(Successor).second)
          Worklist.push_back(Successor);
      }
    }

    errs() << "\n============================\n";
    errs() << "Points-to analysis: " << F.getName() << "\n";
    errs() << "============================\n";
    for (const BasicBlock &B : F)
      printState(B, Out[&B]);
    errs() << "\n";

    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

static bool parsePass(StringRef Name, FunctionPassManager &FPM,
                      ArrayRef<PassBuilder::PipelineElement>) {
  if (Name != "simple-pta")
    return false;
  FPM.addPass(SimplePointsToPass());
  return true;
}

static void registerPass(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(parsePass);
}

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "SimplePointsTo", LLVM_VERSION_STRING,
          registerPass};
}
