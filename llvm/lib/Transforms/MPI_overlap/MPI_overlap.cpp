#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"

using namespace llvm;

static bool ableToMoveIntoComm(Instruction *instrToMove,
                               CallInst *send_recv_instr,
                               StringRef mpi_buf_name);

static bool iterateThruLaterInstr(Function::iterator bb, 
                                  BasicBlock::iterator i, 
                                  Function &Func,
                                  CallInst *send_recv_instr,
                                  CallInst *wait_instr,
                                  StringRef mpi_buf_name);

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct MPI_overlap : public FunctionPass { // this is the pass itself
    // subclass of FunctionPass, which works on a function at a time
    static char ID; // Pass identification, replacement for typeid
    MPI_overlap() : FunctionPass(ID) {} // constructor

    bool runOnFunction(Function &Func) override { 
      bool modified = false;
      // vars used to track send/wait pairs
      CallInst* send_recv_instr = nullptr;
      StringRef mpi_buf_name = "";

      for (Function::iterator bb = Func.begin(), be = Func.end(); bb != be; ++bb) {
        BasicBlock *BB = &(*bb);
        for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i) {
          Instruction* ii = &*i;
          if(isa<CallInst>(ii)) { // check if this instruction is an instance of CallInst
            auto callinst = dyn_cast<CallInst>(ii);
            StringRef call_func_name = callinst->getCalledFunction()->getName();
            if (call_func_name == "MPI_Wait") {
              if (send_recv_instr == nullptr) {
                errs() << "ERROR: found MPI_Wait before MPI_Isend or MPI_Irecv. Not doing anything. \n";
                return false;
              }
              errs() << "[DEBUG] Found MPI wait call: " << *ii << "\n";
              // go through instructions after the call to MPI_Wait
              modified = iterateThruLaterInstr(bb, i, Func, send_recv_instr, callinst, mpi_buf_name);
              // clear states that tracks the current send/recv-wait pair
              send_recv_instr = nullptr;
              mpi_buf_name = "";
            }

            else if (call_func_name == "MPI_Isend") {
                errs() << "[DEBUG] Found MPI send call: " << *ii << "\n";
                // record the fact that we have see a MPI_ISend call
                send_recv_instr = dyn_cast<CallInst>(ii);
                // the first argument of Isend, the buffer to send
                Value *buf_arg = send_recv_instr->getArgOperand(0);
                BitCastInst *bitcast_instr = dyn_cast<BitCastInst>(buf_arg);
                buf_arg = bitcast_instr->getOperand(0); // this should be the name of the buffer
                errs() << " the buffer name is (number): " << buf_arg->getName()  << "\n";
                mpi_buf_name = buf_arg->getName();
            }
            else if (call_func_name == "MPI_Irecv") {
                errs() << "[DEBUG] Found MPI recv call: " << *ii << "\n";
                // record the fact that we have see a MPI_IRecv call
                send_recv_instr = dyn_cast<CallInst>(ii);
                // the first argument of Irecv, the buffer to write to
                //Value *buf_arg = send_recv_instr->getArgOperand(0);
                //BitCastInst *bitcast_instr = dyn_cast<BitCastInst>(buf_arg);
                //buf_arg = bitcast_instr->getOperand(0); // this should be the name of the buffer
                //errs() << " the recv buffer name is (number): " << buf_arg->getName()  << "\n";
                //mpi_buf_name = buf_arg->getName();
            }
          }
        }
      }
      return modified;
    }
  };
}

// iterate through instructions after instruction i in basic block bb. 
// check if any instructions can be moved into the current MPI window
static bool iterateThruLaterInstr(Function::iterator bb, 
                                  BasicBlock::iterator i, // the MPI_Wait() call
                                  Function &Func,
                                  CallInst *send_recv_instr,
                                  CallInst *wait_instr,
                                  StringRef mpi_buf_name) {
  bool modified = false;
  //for (BasicBlock::iterator hoist_i = std::next(i, 1), hoist_e = bb->end(); hoist_i != hoist_e; ++hoist_i) {
  for (BasicBlock::iterator hoist_i = std::next(i, 1), hoist_e = bb->end(); hoist_i != hoist_e;) {
    // operate on the next instruction for potential moveBefore.
    // Even if Inst is moved, we can still procced as usual with hoist_i
    Instruction *Inst = &(*hoist_i); // convert iterator to pointer
    hoist_i++; 
    errs() << "[DEBUG] Instruction after MPI Wait: " << *Inst << "\n";
    //if (isa<BranchInst>(Inst) || isa<CallInst>(Inst) ) {
    //if ( isa<CallInst>(Inst) ) {
    //  errs() << "[DEBUG] Reached call instruction. Stop looking. \n";
    //  return modified;
    //}
    if (ableToMoveIntoComm(Inst, send_recv_instr, mpi_buf_name)) {
        // If ableToMoveIntoComm decides that we cannot move the next instruction
        // into the MPI window, stop looking further. This is to avoid breaking
        // existing dependencies within code regions after the MPI_Wait() call. 
        // If we were able to hoist the next instruction, keep looking.


        // move current instruction before MPI_Wait to expand compute window
        Inst->moveBefore(wait_instr);
        errs() << "[DEBUG] Able to hoist this instruction. Hoisting.\n";
        modified = true;
    } else {
        errs() << "[DEBUG] Cannot move this instruction inside MPI. Stop looking to hoist for this MPI Wait \n";
        return modified;
    }
  }
  // move on to instructions in the following basic blocks
  for (Function::iterator hoist_b = std::next(bb, 1), hoist_be = Func.end(); hoist_b != hoist_be; ++hoist_b) {
    BasicBlock *hoist_BB = &(*hoist_b);
    for (BasicBlock::iterator hoist_i = hoist_BB->begin(), hoist_e = hoist_BB->end(); hoist_i != hoist_e;) {
      Instruction *Inst = &(*hoist_i); // convert iterator to pointer
      hoist_i++; 
      //Instruction *Inst = std::next(hoist_i, 1);
      //hoist_i++; 
      ableToMoveIntoComm(Inst, send_recv_instr, mpi_buf_name);
    }
  }
  return modified;
}
// mpi_buf_name: the buffer name used in previous MPI_Isend/Irecv call. 
static bool ableToMoveIntoComm(Instruction *instrToMove,
                               CallInst *send_recv_instr,
                               StringRef mpi_buf_name) {
  StringRef mpi_call_name = send_recv_instr->getCalledFunction()->getName();
  if (mpi_call_name == "MPI_Isend") {
    // does the current instruction reference mpi_buf_name, which was used in MPI_Isend?
    // We currently only hoist loops that do not write to MPI buffer into the MPI region.

    // Check if this instruction belongs to a loop
    //bool part_of_loop = false;
    //llvm::DominatorTree DT = llvm::DominatorTree();
    //DT.recalculate(*(instrToMove->getFunction()));
    //auto loopInfo = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
    //loopInfo->releaseMemory();
    //loopInfo->analyze(DT);
    //for (auto loop : *loopInfo) {
    //    if (loop->contains(instrToMove)) {
    //        part_of_loop = true;
    //    }
    //}
    //if (part_of_loop) {
    //  errs() << "[DEBUG] This instruction is a part of a loop. \n";
    //}


    // if the curr instr is store AND the second operand is mpi_buf, then cannot move.
    // otherwise, we can move this instr before the MPI_Wait
    if (isa<StoreInst>(instrToMove)) {
      Value *arr_arg = instrToMove->getOperand(1);
      GetElementPtrInst *gep_instr = nullptr;
      if (gep_instr = dyn_cast<GetElementPtrInst>(arr_arg)) {
        //errs() << "  !! found gep instr in store instr: " << *gep_instr << "\n";
        // if the destination of the store instruction is from a gep instruction
        arr_arg = gep_instr->getOperand(0); // get the first argument of gep instr
        if (arr_arg->getName() == mpi_buf_name) {
          errs() << "[DEBUG] Found instruction writting to MPI buffer (cannot hoist): " << arr_arg->getName()  << "\n";
          return false;
        }
      }
    }
    return true;
  }



  return false;

  //unsigned cur_arg = 0;
  //unsigned num_args = send_recv_instr->arg_size();
  //while(cur_arg != num_args) {
  //    Value *arg_val = send_recv_instr->getArgOperand(cur_arg);
  //    cur_arg++;
  //}
  //for (auto arg_i = send_recv_instr->arg_begin(), arg_e = send_recv_instr->arg_end(); arg_i != arg_e; ++arg_i) {
  //}

  //unsigned cur_operand = 0;
  //unsigned num_operands = instrToMove->getNumOperands();
  //while(cur_operand != num_operands) {
  //    Value *operand_val = instrToMove->getOperand(cur_operand);
  //    errs() << "         instrToMove operand " << cur_operand << ": " << *operand_val << "\n";
  //    cur_operand++;
  //}
}

char MPI_overlap::ID = 0;
// register our pass. hello is cmdline arg, Hello World Pass is the pass name
static RegisterPass<MPI_overlap> X("mpi_overlap", "MPI overlap opt Pass");

