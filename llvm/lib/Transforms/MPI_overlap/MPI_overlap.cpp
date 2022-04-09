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
      bool modified = false;
      errs() << "Function : " << Func.getName() << "\n";
      //for (auto &BB : Func) {
      for (Function::iterator bb = Func.begin(), be = Func.end(); bb != be; ++bb) {
        //errs() << "     Basic block : " << BB << "\n";
        BasicBlock *BB = &(*bb);
        for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i) {
          Instruction* ii = &*i;
          if(isa<CallInst>(ii)) { // is this instruction an instance of CallInst?
            auto callinst = dyn_cast<CallInst>(ii);
            StringRef call_func_name = callinst->getCalledFunction()->getName();
            if (call_func_name == "MPI_Wait") {
              errs() << "Found MPI wait call: " << *ii << "\n";
              //Instruction* HoistPoint = BB.getTerminator();
              //ii->moveBefore(HoistPoint);
              //modified = true;

              // go through instructions in the current basic block
              errs() << "instructions in the current bb \n";
  	          for (BasicBlock::iterator hoist_i = std::next(i, 1), hoist_e = bb->end(); hoist_i != hoist_e; ++hoist_i) {
                errs() << "     instr after MPI Wait: " << *hoist_i << "\n";
                unsigned cur_operand = 0;
                unsigned num_operands = hoist_i->getNumOperands();
                while(cur_operand != num_operands) {
                    Value *operand_val = hoist_i->getOperand(cur_operand);
                    errs() << "         instr operand " << cur_operand << ": " << *operand_val << "\n";
                    cur_operand++;
                }
              }
              errs() << "instructions in the followings bbs \n";
              // move on to instructions in the following basic blocks
              for (Function::iterator hoist_b = std::next(bb, 1), hoist_be = Func.end(); hoist_b != hoist_be; ++hoist_b) {
                BasicBlock *hoist_BB = &(*hoist_b);
  	            for (BasicBlock::iterator hoist_i = hoist_BB->begin(), hoist_e = hoist_BB->end(); hoist_i != hoist_e; ++hoist_i) {
                  errs() << "     instr after MPI Wait: " << *hoist_i << "\n";
                  unsigned cur_operand = 0;
                  unsigned num_operands = hoist_i->getNumOperands();
                  while(cur_operand != num_operands) {
                      Value *operand_val = hoist_i->getOperand(cur_operand);
                      errs() << "         instr operand " << cur_operand << ": " << *operand_val << "\n";
                      cur_operand++;
                  }
                }
              }
            }

            //else if (call_func_name == "MPI_Isend") {
            //    errs() << "Found MPI send call: " << *ii << "\n";
            //}
            //else if (call_func_name == "MPI_Irecv") {
            //    errs() << "Found MPI recv call: " << *ii << "\n";
            //}
          }
        }
      }
      return modified;
    }
  };
}

char MPI_overlap::ID = 0;
// register our pass. hello is cmdline arg, Hello World Pass is the pass name
static RegisterPass<MPI_overlap> X("mpi_overlap", "MPI overlap opt Pass");

