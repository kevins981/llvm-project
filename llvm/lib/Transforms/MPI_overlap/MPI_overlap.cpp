#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {
    // Hello - The first implementation, without getAnalysisUsage.
    struct MPI_overlap : public FunctionPass { // this is the pass itself
        // subclass of FunctionPass, which works on a function at a time
        static char ID; // Pass identification, replacement for typeid
        MPI_overlap() : FunctionPass(ID) {} // constructor

        bool runOnFunction(Function &Func) override { 
            //llvm::errs() << "Function : " << F.getName() << "\n";
            //for(llvm::BasicBlock& b : F) {
            //    for (llvm::BasicBlock::iterator DI = b.begin(); DI != b.end(); ) {
            //        llvm::Instruction *I = &*DI++;
            //        llvm::errs() << "Instruction : " << *I << "\n";
            //    }
            //}
            errs() << "Function : " << Func.getName() << "\n";
            for (auto &BB : Func) {
                //errs() << "     Basic block : " << BB << "\n";
                for (BasicBlock::iterator i = BB.begin(), e = BB.end(); i != e; ++i) {
                    Instruction* ii = &*i;
                    errs() << "         Instruction : " << *ii << "\n";
                }
            }
            return false;
        }
    };
}

char MPI_overlap::ID = 0;
// register our pass. hello is cmdline arg, Hello World Pass is the pass name
static RegisterPass<MPI_overlap> X("mpi_overlap", "MPI overlap opt Pass");

