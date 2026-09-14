#include "RegBehaviorAnalyzer.hpp"
#include "DevStructTaintAnalyzer.hpp"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/Transforms/Utils/Local.h"
#include <queue>

std::vector<RegBehavior*>
RegBehaviorAnalyzer::analyze() {
	getBranchesOnAddr();
	extractRegBehavior();
	return this->behaviors;
}

DominatorTree*
RegBehaviorAnalyzer::getDominatorTree(Function* func) {
	if (domTrees.count(func) > 0) {
		return domTrees[func];
	}
	domTrees[func] = new DominatorTree(*func);
	return domTrees[func];
}

PostDominatorTree*
RegBehaviorAnalyzer::getPostDominatorTree(Function* func) {
	if (postDomTrees.count(func) > 0) {
		return postDomTrees[func];
	}
	std::unordered_set<BasicBlock*> removed;
	for (Function::iterator itr = func->begin(); itr != func->end(); ++itr) {
		BasicBlock* BB = &(*itr);
		Instruction* term = BB->getTerminator();
		if (isa<UnreachableInst>(term)) {
			removed.insert(BB);
			termedBBs.insert(BB);
		}
	}
	std::queue<BasicBlock*> workQueue;
	for (BasicBlock* BB : removed) {
		workQueue.push(BB);
	}
	while (!workQueue.empty()) {
		BasicBlock* BB = workQueue.front();
		workQueue.pop();
		bool allSucRemoved = true;
		for (BasicBlock* suc : successors(BB)) {
			if (removed.count(suc) == 0) {
				allSucRemoved = false;
				break;
			}
		}
		if (allSucRemoved) {
			removed.insert(BB);
			termedBBs.insert(BB);
		}
		if (removed.count(BB) > 0) {
			for (BasicBlock* pred : predecessors(BB)) {
				workQueue.push(pred);
			}
		}
	}
	for (BasicBlock* BB : removed) {
		BB->removeFromParent();
	}
	PostDominatorTree* tree = new PostDominatorTree(*func);
	postDomTrees[func] = tree;
	if (!removed.empty()) {
		for (BasicBlock* BB: removed) {
			func->getBasicBlockList().push_back(BB);
		}
	}
	return postDomTrees[func];
}

unsigned
RegBehaviorAnalyzer::assignNextId() {
	return nextId++;
}

unsigned
RegBehaviorAnalyzer::assignNextVarId() {
	return nextVarId++;
}

Var*
RegBehaviorAnalyzer::getVarByValueType(ValueType* type, bool isRead, Instruction* ins) {
	for (Var* var : vars) {
		if (var->getIns() == ins) {
			return var;
		}
	}
	Var* newVar = new Var(assignNextVarId(), isRead, type, ins);
	vars.push_back(newVar);
	return newVar;
}

void
RegBehaviorAnalyzer::collectBBsInPath(BasicBlockSet& visited, BasicBlock* cur, BasicBlock* end,
									  std::vector<BasicBlock*>& path, BasicBlockSet& result) {
	if (cur == end) {
		result.insert(path.begin(), path.end());
		return;
	}
	if (visited.count(cur) > 0) {
		return;
	}
	visited.insert(cur);
	path.push_back(cur);
	Instruction* term = cur->getTerminator();
	for (unsigned i = 0; i < term->getNumSuccessors(); ++i) {
		BasicBlock* suc = term->getSuccessor(i);
		collectBBsInPath(visited, suc, end, path, result);
	}
	path.pop_back();
}

BasicBlockSet
RegBehaviorAnalyzer::getPredBBs(BasicBlock* start, BasicBlock* end) {
	BasicBlockSet res;
	BasicBlockSet visited;
	std::vector<BasicBlock*> path;
	collectBBsInPath(visited, start, end, path, res);
	return res;
}

void
RegBehaviorAnalyzer::removeBBsFromSet(BasicBlockSet& set, BasicBlockSet& removed) {
	for (BasicBlock* BB : removed) {
		set.erase(BB);
	}
}

void
RegBehaviorAnalyzer::getBranchesOnAddr() {
	for (FuncArg* funcArg : simFuncs) {
		TaintAnalyzer analyzer(funcArg->sim, funcArg->sim->getArg(funcArg->idx));
		analyzer.analyze();
		std::unordered_set<Value*> taints = analyzer.getTaintedValues();
		for (Value* taint : taints) {
			for (Value::user_iterator i = taint->user_begin(); i != taint->user_end(); ++i) {
				if (isa<CmpInst>(*i) || isa<SwitchInst>(*i)) {
					Instruction* ins = cast<Instruction>(*i);
					if (isa<CmpInst>(ins)) {
						for (Instruction::user_iterator j = ins->user_begin(); j != ins->user_end(); ++j) {
							User* user = *j;
							if (isa<PHINode>(user) || isa<SelectInst>(user) || isa<ReturnInst>(user) || isa<BinaryOperator>(user)
							    || isa<CastInst>(user)) {
								continue;
							}
							if (!isa<BranchInst>(user)) {
								user->print(errs());
								errs() << "\n";
							}
							assert(isa<BranchInst>(user) && "user is not br\n");
							BranchInst* br = cast<BranchInst>(user);
							addrCheckers.insert(br);
						}
					} else {
						addrCheckers.insert(ins);
					}
				} else if (isa<GetElementPtrInst>(*i)){
					// Check whether it is used as an indirect call index
					GetElementPtrInst* GEP = cast<GetElementPtrInst>(*i);
					// Whether it is used as an index
					bool isIdx = false;
					for (auto idxItr = GEP->idx_begin(); idxItr != GEP->idx_end(); ++idxItr) {
						if (taint == idxItr->get()) {
							isIdx = true;
						}
					}
					if (!isIdx) {
						continue;
					}
					DevStructTaintAnalyzer taintAnalyzer(GEP->getFunction(), GEP);
					taintAnalyzer.analyze();
					std::unordered_set<Value*> gepAliases = taintAnalyzer.getAliases();
					for (Value* gepAlias : gepAliases) {
						for (auto userItr = gepAlias->user_begin(); userItr != gepAlias->user_end(); ++userItr) {
							if (isa<GetElementPtrInst>(*userItr)) {
								GetElementPtrInst* aliasGEP = cast<GetElementPtrInst>(*userItr);
								if (aliasGEP->getPointerOperand() == gepAlias) {
									DevStructTaintAnalyzer gepTaintAnalyzer(aliasGEP->getFunction(), aliasGEP);
									gepTaintAnalyzer.analyze();
									std::unordered_set<Value*> writeAliases = gepTaintAnalyzer.getAliases();
									for (Value* writeAlias : writeAliases) {
										for (auto writeItr = writeAlias->user_begin(); writeItr != writeAlias->user_end(); ++writeItr) {
											if (isa<CallBase>(*writeItr)) {
												CallBase* call = cast<CallBase>(*writeItr);
												if (call->getCalledOperand() == writeAlias) {
													addrCheckers.insert(call);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

bool
RegBehaviorAnalyzer::changeStateBeforeMerging(BasicBlock* start, BasicBlock* groupPoint) {
	std::queue<BasicBlock*> blockQueue;
	blockQueue.push(start);
	BasicBlockSet visitedBBs;
	while (!blockQueue.empty()) {
		BasicBlock* BB = blockQueue.front();
		if (BB != groupPoint && bbChangeState[BB]) {
			return true;
		}
		blockQueue.pop();
		if (visitedBBs.count(BB) == 0 && BB != groupPoint) {
			visitedBBs.insert(BB);
			Instruction* term = BB->getTerminator();
			for (unsigned j = 0; j < term->getNumSuccessors(); ++j) {
				blockQueue.push(term->getSuccessor(j));
			}
		}
	}
	return false;
}

std::vector<RegBehavior*>
RegBehaviorAnalyzer::getRegBehaviorFromBranch(Instruction* addrChecker, BasicBlock* start, BasicBlockSet& bbs,
											  std::unordered_set<Instruction*>& addrCheckerVisited,
											  BasicBlockSet& predBBs,
											  std::unordered_set<Function*>& funcVisited, bool isBBLimit) {
	Function* func = start->getParent();
	funcVisited.insert(func);
	std::vector<RegBehavior*> res;
	BasicBlockSet endBBs;
	BasicBlockSet visitedBBs;
	std::queue<BasicBlock*> workQueue;
	workQueue.push(start);
	bool hasSubBehavior = false;
	bool changeState = false;
	std::unordered_map<BasicBlock*, std::vector<Var*>> accessedVars;
	while (!workQueue.empty()) {
		BasicBlock* BB = workQueue.front();
		workQueue.pop();
		if (visitedBBs.count(BB) == 0) {
			visitedBBs.insert(BB);
			bbChangeState[BB] = false;
			for (BasicBlock::iterator itr = BB->begin(); itr != BB->end(); ++itr) {
				Instruction* ins = &(*itr);
				// Check sub behaviors
				if (addrCheckers.count(ins) > 0) {
					hasSubBehavior = true;
					if (addrCheckerVisited.count(ins) == 0) {
						BasicBlockSet predBBsLocal = getPredBBs(start, BB);
						predBBs.insert(predBBsLocal.begin(), predBBsLocal.end());
						std::vector<RegBehavior*> subRes = checkAddrChecker(ins, addrCheckerVisited, predBBsLocal, 
																			funcVisited);
						removeBBsFromSet(predBBs, predBBsLocal);
						res.insert(res.end(), subRes.begin(), subRes.end());
					}
				}
				if (isa<LoadInst>(ins)) {
					LoadInst* load = cast<LoadInst>(ins);
					Value* pointer = load->getPointerOperand();
					if (typeAnalyzer->fieldOfTypes(pointer, stateTypeNames)) {
						Var* var = getVarByValueType(typeAnalyzer->getValueVType(pointer), true, load);
						bbAccessedVars[BB].push_back(var);
						accessedVars[BB].push_back(var);
					}
				}
				if (isa<StoreInst>(ins)) {
					StoreInst* store = cast<StoreInst>(ins);
					Value* pointer = store->getPointerOperand();
					if (typeAnalyzer->fieldOfTypes(pointer, stateTypeNames)) {
						Var* var = getVarByValueType(typeAnalyzer->getValueVType(pointer), false, store);
						bbAccessedVars[BB].push_back(var);
						accessedVars[BB].push_back(var);
						changeState = true;
						bbChangeState[BB] = true;
					}
				}
				if (isa<CallInst>(ins)) {
					CallInst* call = cast<CallInst>(ins);
					// Only analyze the callees with state args
					bool passState = false;
					for (unsigned i = 0; i < call->arg_size(); ++i) {
						Value* arg = call->getArgOperand(i);
						if (typeAnalyzer->fieldOfTypes(arg, stateTypeNames)) {
							passState = true;
							break;
						}
					}
					if (passState) {
						// Analyze the callee
						if (!call->isIndirectCall() && !call->isInlineAsm()) {
							Function* callee = call->getCalledFunction();
							if (!callee->isIntrinsic() && !callee->isDeclaration()) {
								if (funcHasAddrChecker.count(callee) > 0 && funcHasAddrChecker[callee]) {
									hasSubBehavior = true;
								}
								if (funcChangeState.count(callee) > 0 && funcChangeState[callee]) {
									changeState = true;
									bbChangeState[BB] = true;
								}
								if (funcHasAddrChecker.count(callee) == 0 || funcAccessedVars.count(callee) == 0
									|| funcChangeState.count(callee) == 0) {
									// Visit the callee
									if (funcVisited.count(callee) == 0) {
										// Not visited before
										BasicBlockSet emptyBBs;
										BasicBlockSet predBBsLocal = getPredBBs(start, BB);
										predBBs.insert(predBBsLocal.begin(), predBBsLocal.end());
										
										std::vector<RegBehavior*> subRes = getRegBehaviorFromBranch(addrChecker,
																									&(callee->getEntryBlock()),
																									emptyBBs, addrCheckerVisited,
																									predBBs,
																									funcVisited, false);
										removeBBsFromSet(predBBs, predBBsLocal);
										if (!subRes.empty()) {
											hasSubBehavior = true;
											res.insert(res.end(), subRes.begin(), subRes.end());
										}
										if (funcChangeState[callee]) {
											changeState = true;
											bbChangeState[BB] = true;
										}
									}
									// Else it is waiting for a result of its callee
								}
								// Visited
								if (funcAccessedVars.count(callee) > 0) {
									std::unordered_map<BasicBlock*, std::vector<Var*>> subVars = funcAccessedVars[callee];
									for (auto itr : subVars) {
										BasicBlock* funcBB = itr.first;
										std::vector<Var*> funcVars = itr.second;
										bbAccessedVars[funcBB] = funcVars;
										visitedBBs.insert(funcBB);
										accessedVars[funcBB] = funcVars;
									}
								}
							}
						}
					}
				}
			}
			Instruction* term = BB->getTerminator();
			for (unsigned i = 0; i < term->getNumSuccessors(); ++i) {
				BasicBlock* suc = term->getSuccessor(i);
				if (isBBLimit && bbs.count(suc) == 0) {
					endBBs.insert(BB);
				} else if (visitedBBs.count(suc) == 0) {
					workQueue.push(suc);
				}
			}
		}
	}
	// No BB limit -> a function visit
	if (!isBBLimit) {
		funcHasAddrChecker[func] = hasSubBehavior;
		funcChangeState[func] = changeState;
		funcAccessedVars[func] = accessedVars;
	} else if (!hasSubBehavior && changeState) {
		// Has BB limit -> an address checker, may get a register behavior
		// The last instruction before the point is the end
		Instruction* begin = &(*(start->begin()));
		std::unordered_set<Instruction*> end;
		for (BasicBlock* BB : endBBs) {
			end.insert(BB->getTerminator());
		}
		std::unordered_map<BasicBlock*, std::vector<Var*>> regAccessedVars;
		for (BasicBlock* BB : visitedBBs) {
			regAccessedVars[BB] = bbAccessedVars[BB];
		}
		for (BasicBlock* BB : predBBs) {
			regAccessedVars[start].insert(regAccessedVars[start].end(), bbAccessedVars[BB].begin(), bbAccessedVars[BB].end());
		}
		res.push_back(new RegBehavior(assignNextId(), addrChecker, begin, end, regAccessedVars));
	}
	return res;
}

std::vector<RegBehavior*>
RegBehaviorAnalyzer::checkAddrChecker(Instruction* addrChecker, std::unordered_set<Instruction*>& addrCheckerVisited,
									  BasicBlockSet& predBBs,
									  std::unordered_set<Function*>& funcVisited) {
	std::vector<RegBehavior*> res;
	addrCheckerVisited.insert(addrChecker);
	std::vector<std::vector<RegBehavior*>> brSubRes;
	if (isa<CmpInst>(addrChecker) || isa<SwitchInst>(addrChecker)) {
		if (!addrChecker->isTerminator()) {
			addrChecker->print(errs());
			errs() << "\n";
		}
		assert(addrChecker->isTerminator() && "Addr checker is not a terminator\n");
		// Use the post dominator tree to find the merge point.
		PostDominatorTree* tree = getPostDominatorTree(addrChecker->getFunction());
		if (tree == NULL) {
			errs() << "No post dom tree\n";
			assert(false);
		}
		DomTreeNodeBase<BasicBlock>* checkerNode = tree->getNode(addrChecker->getParent());
		BasicBlock* mergePoint = checkerNode->getIDom()->getBlock();
		assert(mergePoint != NULL && "No merge point!");
		// Find the group points of the successors.
		GroupMap groupPoints;
		for (unsigned i = 0; i < addrChecker->getNumSuccessors(); ++i) {
			BasicBlock* suci = addrChecker->getSuccessor(i);
			if (termedBBs.count(suci) > 0) {
				continue;
			}
			for (unsigned j = i + 1; j < addrChecker->getNumSuccessors(); ++j) {
				BasicBlock* sucj = addrChecker->getSuccessor(j);
				if (termedBBs.count(sucj) > 0) {
					continue;
				}
				BasicBlock* commonDom = tree->findNearestCommonDominator(suci, sucj);
				if (commonDom != mergePoint) {
					groupPoints[std::make_pair(i, j)] = commonDom;
				}
			}
		}
		std::vector<BasicBlockSet> bbsForEachBr;
		// For each successor, find its coarse register behavior's BBs.
		std::queue<BasicBlock*> blockQueue;
		for (unsigned i = 0; i < addrChecker->getNumSuccessors(); ++i) {
			BasicBlockSet bbsInBranch;
			assert(blockQueue.empty() && "Visit block merge point no empty\n");
			blockQueue.push(addrChecker->getSuccessor(i));
			while (!blockQueue.empty()) {
				BasicBlock* BB = blockQueue.front();
				blockQueue.pop();
				if (bbsInBranch.count(BB) == 0 && BB != mergePoint) {
					bbsInBranch.insert(BB);
					Instruction* term = BB->getTerminator();
					for (unsigned j = 0; j < term->getNumSuccessors(); ++j) {
						blockQueue.push(term->getSuccessor(j));
					}
				}
			}
			bbsForEachBr.push_back(bbsInBranch);
		}
		// Analyze each branch to get the reg behaviors
		for (unsigned i = 0; i < addrChecker->getNumSuccessors(); ++i) {
			std::vector<RegBehavior*> subRes = getRegBehaviorFromBranch(addrChecker, addrChecker->getSuccessor(i), bbsForEachBr[i],
																		addrCheckerVisited, predBBs, funcVisited, true);
			brSubRes.push_back(subRes);
		}
		// Merge register behaviors if they are semantically equal
		for (auto itr : groupPoints) {
			std::pair<unsigned, unsigned> indices = itr.first;
			BasicBlock* groupPoint = itr.second;
			std::vector<RegBehavior*>& behaviors1 = brSubRes[indices.first];
			std::vector<RegBehavior*>& behaviors2 = brSubRes[indices.second];
			if (behaviors1.empty() || behaviors2.empty()) {
				continue;
			}
			BasicBlock* suc1 = addrChecker->getSuccessor(indices.first);
			BasicBlock* suc2 = addrChecker->getSuccessor(indices.second);
			if (!changeStateBeforeMerging(suc1, groupPoint) && !changeStateBeforeMerging(suc2, groupPoint)) {
				for (RegBehavior* behavior1 : behaviors1) {
					for (RegBehavior* behavior2 : behaviors2) {
						for (Instruction* begin : behavior2->getBegin()) {
							behavior1->addBegin(begin);
						}
					}
				}
				behaviors2.clear();
			}
		}
	} else if (isa<CallBase>(addrChecker)) {
		CallBase* call = cast<CallBase>(addrChecker);
		std::unordered_set<Function*> targets = indirectCallAnalyzer->getTarget(call);
		for (Function* target : targets) {
			errs() << target->getName() << "\n";
			BasicBlockSet BBs;
			for (auto itr = target->begin(); itr != target->end(); ++itr) {
				BBs.insert(&(*itr));
			}
			std::vector<RegBehavior*> subRes = getRegBehaviorFromBranch(addrChecker, &target->getEntryBlock(), BBs,
																		addrCheckerVisited, predBBs, funcVisited, true);
			brSubRes.push_back(subRes);
		}
	}
	for (std::vector<RegBehavior*> brRes : brSubRes) {
		res.insert(res.end(), brRes.begin(), brRes.end());
	}
	return res;
}

void
RegBehaviorAnalyzer::extractRegBehavior() {
	std::unordered_set<Instruction*> addrCheckerVisited;
	std::unordered_set<Function*> funcVisited;
	for (Instruction* addrChecker : addrCheckers) {
		if (addrCheckerVisited.count(addrChecker) > 0) {
			continue;
		}
		BasicBlockSet predBBs;
		std::vector<RegBehavior*> res = checkAddrChecker(addrChecker, addrCheckerVisited, predBBs, funcVisited);
		behaviors.insert(behaviors.end(), res.begin(), res.end());
	}
	for (RegBehavior* behavior : behaviors) {
		errs() << behavior->getId() << "\n";
		behavior->getAddrChecker()->print(errs());
		errs() << " addrChecker\n";
		printDebugLineAndCol(behavior->getAddrChecker());
		errs() << behavior->getAddrChecker()->getFunction()->getName() << "\n";
		for (Instruction* begin : behavior->getBegin()) {
			begin->print(errs());
			errs() << " begin\n";
			printDebugLineAndCol(begin);
		}
		for (Instruction* end : behavior->getEnd()) {
			end->print(errs());
			errs() << " end\n";
			printDebugLineAndCol(end);
		}
	}
}

void
RegBehaviorAnalyzer::printDebugLineAndCol(Instruction* ins) const {
	const DebugLoc loc = ins->getDebugLoc();
	if (loc) {
		errs() << "line " << loc.getLine() << " col " << loc.getCol();
		MDNode* node = loc.getScope();
		if (isa<DIScope>(node)) {
			DIScope* scope = cast<DIScope>(node);
			if (const DIFile *file = scope->getFile()) {
				errs() << " file: " << file->getFilename() << "\n";
			}
		}
	}
}
