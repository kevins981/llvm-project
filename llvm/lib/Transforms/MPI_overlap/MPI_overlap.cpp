#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace {
    // Hello - The first implementation, without getAnalysisUsage.
    struct MPI_overlap : public FunctionPass { // this is the pass itself
        // subclass of FunctionPass, which works on a function at a time
        static char ID; // Pass identification, replacement for typeid
        MPI_overlap() : FunctionPass(ID) {} // constructor

        bool runOnFunction(Function &Func) override { 
            errs() << "Function : " << Func.getName() << "\n";
            for (auto &BB : Func) {
                //errs() << "     Basic block : " << BB << "\n";
                for (BasicBlock::iterator i = BB.begin(), e = BB.end(); i != e; ++i) {
                    Instruction* ii = &*i;
                    if(isa<CallInst>(ii)) { // is this instruction an instance of CallInst?
                        auto callinst = dyn_cast<CallInst>(ii);
                        StringRef call_func_name = callinst->getCalledFunction()->getName();
                        if (call_func_name == "MPI_Wait") {
                            errs() << "Found MPI wait call: " << *ii << "\n";
                        }
                        else if (call_func_name == "MPI_Isend") {
                            errs() << "Found MPI send call: " << *ii << "\n";
                        }
                        else if (call_func_name == "MPI_Irecv") {
                            errs() << "Found MPI recv call: " << *ii << "\n";
                        }
                    }
                }
            }
            return false;
        }
    };
}

char MPI_overlap::ID = 0;
// register our pass. hello is cmdline arg, Hello World Pass is the pass name
static RegisterPass<MPI_overlap> X("mpi_overlap", "MPI overlap opt Pass");

