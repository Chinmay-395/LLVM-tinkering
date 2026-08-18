#include <iostream>
#include <memory>

// LLVM core headers
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path-to-file.ll>\n";
        return 1;
    }

    // 1. LLVMContext manages core global data structures (types, constants table)
    LLVMContext context;
    SMDiagnostic error; // Captures parser errors if the .ll file is invalid

    // 2. Parse the .ll file into a Module object in memory
    std::unique_ptr<Module> mod = parseIRFile(argv[1], error, context);
    if (!mod)
    {
        error.print(argv[0], errs());
        return 1;
    }

    std::cout << "Successfully parsed module: " << mod->getName().str() << "\n\n";

    // 3. Traverse: Module -> Functions
    for (Function &F : *mod)
    {
        std::cout << "Function: @" << F.getName().str() << "\n";
        std::cout << "  Return Type: ";
        F.getReturnType()->print(outs());
        std::cout << "\n";
        std::cout << "  Argument Count: " << F.arg_size() << "\n";

        // 4. Traverse: Function -> BasicBlocks
        for (BasicBlock &BB : F)
        {
            std::cout << "  [BasicBlock: " << BB.getName().str() << "]\n";

            // 5. Traverse: BasicBlock -> Instructions
            for (Instruction &I : BB)
            {
                // Print opcode name (e.g., "alloca", "load", "add")
                std::cout << "    Instruction: " << I.getOpcodeName();

                // Print the full instruction line
                std::cout << "  -->  ";
                I.print(outs());
                std::cout << "\n";
            }
        }
        std::cout << "-------------------------------------------\n";
    }

    return 0;
}