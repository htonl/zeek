// See the file "COPYING" in the main distribution directory for copyright.

#pragma once

#include "Stmt.h"
#include "Val.h"
#include "Event.h"
#include "ReachingDefs.h"
#include "UseDefs.h"


class NameExpr;
class ConstExpr;
class FieldExpr;
class ListExpr;
class EventHandler;
class Stmt;
class SwitchStmt;

// Class representing a single compiled statement.  Designed to
// be fully opaque, but also effective without requiring pointer
// management.
class CompiledStmt {
protected:
	friend class AbstractMachine;

	CompiledStmt(int _stmt_num)	{ stmt_num = _stmt_num; }

	int stmt_num;	// used for AbstractMachine
};


class OpaqueVals;
typedef std::vector<IntrusivePtr<Val>> val_vec;

// The (reduced) statement currently being compiled.  Used for both
// tracking "use" and "reaching" definitions, and for error messages.
extern const Stmt* curr_stmt;

class Compiler : public Stmt {
public:
	void SetCurrStmt(const Stmt* stmt)	{ curr_stmt = stmt; }

#include "CompilerBaseDefs.h"

	virtual const CompiledStmt InterpretExpr(const Expr* e) = 0;
	virtual const CompiledStmt InterpretExpr(const NameExpr* n,
							const Expr* e) = 0;

	virtual const CompiledStmt ArithCoerce(const NameExpr* n,
							const Expr* e) = 0;
	virtual const CompiledStmt RecordCoerce(const NameExpr* n,
							const Expr* e) = 0;
	virtual const CompiledStmt TableCoerce(const NameExpr* n,
							const Expr* e) = 0;
	virtual const CompiledStmt VectorCoerce(const NameExpr* n,
							const Expr* e) = 0;

	virtual const CompiledStmt IfElse(const NameExpr* n, const Stmt* s1,
						const Stmt* s2) = 0;

	virtual const CompiledStmt While(const Stmt* cond_stmt,
				const NameExpr* cond, const Stmt* body) = 0;
	virtual const CompiledStmt Loop(const Stmt* body) = 0;

	virtual const CompiledStmt When(Expr* cond, const Stmt* body,
				const Expr* timeout, const Stmt* timeout_body,
				bool is_return) = 0;

	virtual const CompiledStmt Switch(const SwitchStmt* sw) = 0;

	virtual const CompiledStmt For(const ForStmt* f) = 0;

	virtual const CompiledStmt Call(const ExprStmt* e) = 0;
	virtual const CompiledStmt AssignToCall(const ExprStmt* e) = 0;

	virtual const CompiledStmt AssignVecElems(const Expr* e) = 0;

	virtual const CompiledStmt InitRecord(ID* id, RecordType* rt) = 0;
	virtual const CompiledStmt InitVector(ID* id, VectorType* vt) = 0;
	virtual const CompiledStmt InitTable(ID* id, TableType* tt,
						Attributes* attrs) = 0;

	virtual const CompiledStmt Next() = 0;
	virtual const CompiledStmt Break() = 0;
	virtual const CompiledStmt FallThrough() = 0;

	virtual const CompiledStmt StartingBlock() = 0;
	virtual const CompiledStmt FinishBlock(const CompiledStmt start) = 0;

	virtual bool NullStmtOK() const = 0;

	virtual const CompiledStmt EmptyStmt() = 0;
	virtual const CompiledStmt ErrorStmt() = 0;

	virtual bool IsUnused(const ID* id, const Stmt* where) const = 0;

	// Called to synchronize any globals that have been modified
	// prior to switching to execution out of the current function
	// body (for a call or a return).  The argument is a statement
	// or expression, used to find reaching-defs.  A nil value
	// corresponds to "running off the end" (no explicit return).
	virtual void SyncGlobals(const BroObj* o) = 0;

	// Returns a handle to state associated with building
	// up a list of values.
	virtual OpaqueVals* BuildVals(const IntrusivePtr<ListExpr>&) = 0;

protected:
	TraversalCode Traverse(TraversalCallback* cb) const override;
};


struct function_ingredients;
class CallExpr;
class Func;
class Body;
class UseDefs;
class ProfileFunc;
class AbstractStmt;
union AS_ValUnion;

// Maps frame slots to associated identifiers.
typedef std::vector<const ID*> frame_map;

class AbstractMachine : public Compiler {
public:
	AbstractMachine(const BroFunc* f, Stmt* body,
			UseDefs* ud, Reducer* rd, ProfileFunc* pf);
	~AbstractMachine() override;

	Stmt* CompileBody();

#include "CompilerSubDefs.h"

	const CompiledStmt InterpretExpr(const Expr* e) override;
	const CompiledStmt InterpretExpr(const NameExpr* n,
						const Expr* e) override;

	const CompiledStmt ArithCoerce(const NameExpr* n,
					const Expr* e) override;
	const CompiledStmt RecordCoerce(const NameExpr* n,
					const Expr* e) override;
	const CompiledStmt TableCoerce(const NameExpr* n,
					const Expr* e) override;
	const CompiledStmt VectorCoerce(const NameExpr* n,
					const Expr* e) override;

	const CompiledStmt IfElse(const NameExpr* n, const Stmt* s1,
					const Stmt* s2) override;

	const CompiledStmt While(const Stmt* cond_stmt, const NameExpr* cond,
					const Stmt* body) override;
	const CompiledStmt Loop(const Stmt* body) override;

	const CompiledStmt When(Expr* cond, const Stmt* body,
			const Expr* timeout, const Stmt* timeout_body,
			bool is_return) override;

	const CompiledStmt Switch(const SwitchStmt* sw) override;

	const CompiledStmt For(const ForStmt* f) override;

	const CompiledStmt Call(const ExprStmt* e) override;
	const CompiledStmt AssignToCall(const ExprStmt* e) override;

	const CompiledStmt AssignVecElems(const Expr* e) override;

	const CompiledStmt LoopOverTable(const ForStmt* f, const NameExpr* val);
	const CompiledStmt LoopOverVector(const ForStmt* f, const NameExpr* val);
	const CompiledStmt LoopOverString(const ForStmt* f, const NameExpr* val);

	const CompiledStmt FinishLoop(AbstractStmt iter_stmt, const Stmt* body,
					int info_slot);

	const CompiledStmt InitRecord(ID* id, RecordType* rt) override;
	const CompiledStmt InitVector(ID* id, VectorType* vt) override;
	const CompiledStmt InitTable(ID* id, TableType* tt, Attributes* attrs)
		override;

	const CompiledStmt Next() override	{ return GenGoTo(nexts); }
	const CompiledStmt Break() override	{ return GenGoTo(breaks); }
	const CompiledStmt FallThrough() override
		{ return GenGoTo(fallthroughs); }

	const CompiledStmt StartingBlock() override;
	const CompiledStmt FinishBlock(const CompiledStmt start) override;

	bool NullStmtOK() const override;

	const CompiledStmt EmptyStmt() override;
	const CompiledStmt ErrorStmt() override;

	bool IsUnused(const ID* id, const Stmt* where) const override;

	void SyncGlobals(const BroObj* o) override;

	// Sync's the given global at the given location 'o'.  Third argument
	// provides the RDs at entry to the body.
	void SyncGlobal(ID* g, const BroObj* o, const RD_ptr& entry_rds);

	OpaqueVals* BuildVals(const IntrusivePtr<ListExpr>&) override;

	IntrusivePtr<Val> Exec(Frame* f, stmt_flow_type& flow) const override;

	int FrameSlot(const NameExpr* id);

	void StmtDescribe(ODesc* d) const override;

	void Dump();

protected:
	void Init();

	friend class ResumptionAM;

	IntrusivePtr<Val> DoExec(Frame* f, int start_pc,
					stmt_flow_type& flow) const;

	int InternalBuildVals(const ListExpr*);

	const CompiledStmt CompileInExpr(const NameExpr* n1, const NameExpr* n2,
						const NameExpr* n3)
		{ return CompileInExpr(n1, n2, nullptr, n3, nullptr); }

	const CompiledStmt CompileInExpr(const NameExpr* n1, const NameExpr* n2,
						const ConstExpr* c)
		{ return CompileInExpr(n1, n2, nullptr, nullptr, c); }

	const CompiledStmt CompileInExpr(const NameExpr* n1, const ConstExpr* c,
						const NameExpr* n3)
		{ return CompileInExpr(n1, nullptr, c, n3, nullptr); }

	const CompiledStmt CompileInExpr(const NameExpr* n1, const ListExpr* l,
						const NameExpr* n2);

	// In the following, one of n2 or c2 (likewise, n3/c3) will be nil.
	const CompiledStmt CompileInExpr(const NameExpr* n1,
				const NameExpr* n2, const ConstExpr* c2,
				const NameExpr* n3, const ConstExpr* c3);

	const CompiledStmt CompileIndex(const NameExpr* n1, const NameExpr* n2,
					const ListExpr* l);

	const CompiledStmt DoCall(const CallExpr* c, const NameExpr* n, UDs uds);

	const CompiledStmt CompileSchedule(const NameExpr* n,
					const ConstExpr* c, int is_interval,
					EventHandler* h, const ListExpr* l);

	const CompiledStmt CompileEvent(EventHandler* h, const ListExpr* l);

	const CompiledStmt ValueSwitch(const SwitchStmt* sw, const NameExpr* v,
					const ConstExpr* c);
	const CompiledStmt TypeSwitch(const SwitchStmt* sw, const NameExpr* v,
					const ConstExpr* c);

	ListVal* ValVecToListVal(val_vec* v, int n) const;

	void ResolveNexts(const CompiledStmt s)
		{ ResolveGoTos(nexts, s); }
	void ResolveBreaks(const CompiledStmt s)
		{ ResolveGoTos(breaks, s); }
	void ResolveFallThroughs(const CompiledStmt s)
		{ ResolveGoTos(fallthroughs, s); }

	void ResolveGoTos(vector<int>& gotos, const CompiledStmt s);

	CompiledStmt GenGoTo(vector<int>& v);
	CompiledStmt GoTo();
	CompiledStmt GoTo(const CompiledStmt s);
	CompiledStmt GoToTarget(const CompiledStmt s);
	CompiledStmt GoToTargetBeyond(const CompiledStmt s);
	CompiledStmt PrevStmt(const CompiledStmt s);
	void SetV1(CompiledStmt s, const CompiledStmt s1);
	void SetV2(CompiledStmt s, const CompiledStmt s2);
	void SetV3(CompiledStmt s, const CompiledStmt s2);
	void SetGoTo(CompiledStmt s, const CompiledStmt targ)
		{ SetV1(s, targ); }

	const CompiledStmt AddStmt(const AbstractStmt& stmt);
	AbstractStmt& TopStmt();

	// Returns the last (interpreter) statement in the body.
	const Stmt* LastStmt() const;

	void FlushVars(const Expr* e);

	void LoadParam(ID* id)		{ LoadOrStoreLocal(id, true, true); }
	void LoadGlobal(ID* id)		{ LoadOrStoreGlobal(id, true, true); }

	void StoreLocal(ID* id)		{ LoadOrStoreLocal(id, false, false); }
	void StoreGlobal(ID* id)	{ LoadOrStoreGlobal(id, false, false); }

	const CompiledStmt LoadOrStoreLocal(ID* id, bool is_load, bool add);
	const CompiledStmt LoadOrStoreGlobal(ID* id, bool is_load, bool add);

	int AddToFrame(const ID*);

	int FrameSlot(const ID* id);
	bool HasFrameSlot(const ID* id) const;

	int NewSlot();
	int RegisterSlot();

	vector<AbstractStmt> stmts;

	// Indices of break/next/fallthrough goto's, so they can be
	// patched up post factor.
	vector<int> breaks;
	vector<int> nexts;
	vector<int> fallthroughs;

	const BroFunc* func;
	Stmt* body;
	UseDefs* ud;
	Reducer* reducer;
	ProfileFunc* pf;

	// Maps identifiers to their frame location.
	std::unordered_map<const ID*, int> frame_layout;

	// Inverse mapping, used for dumping statements.
	frame_map frame_denizens;

	// Which frame slots need clearing/deleting on entry/exit,
	// and their corresponding type tags.
	std::vector<int> managed_slots;
	std::vector<TypeTag> managed_slot_types;

	// The following are used for switch statements, mapping the
	// switch value (which can be any atomic type) to a branch target.
	// We have vectors of them because functions can contain multiple
	// switches.
	template<class T> using CaseMap = std::map<T, int>;
	template<class T> using CaseMaps = std::vector<CaseMap<T>>;

	CaseMaps<bro_int_t> int_cases;
	CaseMaps<bro_uint_t> uint_cases;
	CaseMaps<double> double_cases;

	// Note, we use this not only for strings but for addresses
	// and prefixes.
	CaseMaps<std::string> str_cases;

	void DumpIntCases(int i) const;
	void DumpUIntCases(int i) const;
	void DumpDoubleCases(int i) const;
	void DumpStrCases(int i) const;

	int frame_size;
	int register_slot;
	bool error_seen = false;
};

// This is a statement that resumes execution into a code block in an
// AbstractMachine.  Used for deferred execution for "when" statements.
class ResumptionAM : public Stmt {
public:
	ResumptionAM(const AbstractMachine* _am, int _xfer_pc)
		{
		am = _am;
		xfer_pc = _xfer_pc;
		}

	IntrusivePtr<Val> Exec(Frame* f, stmt_flow_type& flow) const override;

	void StmtDescribe(ODesc* d) const override;

protected:
	TraversalCode Traverse(TraversalCallback* cb) const override;

	const AbstractMachine* am;
	int xfer_pc = 0;
};
