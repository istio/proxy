// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_GRAPH_H_
#define V8_COMPILER_TURBOSHAFT_GRAPH_H_

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

#include "src/base/iterator.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/source-position.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

class Assembler;
class VarAssembler;

// `OperationBuffer` is a growable, Zone-allocated buffer to store Turboshaft
// operations. It is part of a `Graph`.
// The buffer can be seen as an array of 8-byte `OperationStorageSlot` values.
// The structure is append-only, that is, we only add operations at the end.
// There are rare cases (i.e., loop phis) where we overwrite an existing
// operation, but only if we can guarantee that the new operation is not bigger
// than the operation we overwrite.
class OperationBuffer {
 public:
  // A `ReplaceScope` is to overwrite an existing operation.
  // It moves the end-pointer temporarily so that the next emitted operation
  // overwrites an old one.
  class ReplaceScope {
   public:
    ReplaceScope(OperationBuffer* buffer, OpIndex replaced)
        : buffer_(buffer),
          replaced_(replaced),
          old_end_(buffer->end_),
          old_slot_count_(buffer->SlotCount(replaced)) {
      buffer_->end_ = buffer_->Get(replaced);
    }
    ~ReplaceScope() {
      DCHECK_LE(buffer_->SlotCount(replaced_), old_slot_count_);
      buffer_->end_ = old_end_;
      // Preserve the original operation size in case it has become smaller.
      buffer_->operation_sizes_[replaced_.id()] = old_slot_count_;
      buffer_->operation_sizes_[OpIndex(replaced_.offset() +
                                        static_cast<uint32_t>(old_slot_count_) *
                                            sizeof(OperationStorageSlot))
                                    .id() -
                                1] = old_slot_count_;
    }

    ReplaceScope(const ReplaceScope&) = delete;
    ReplaceScope& operator=(const ReplaceScope&) = delete;

   private:
    OperationBuffer* buffer_;
    OpIndex replaced_;
    OperationStorageSlot* old_end_;
    uint16_t old_slot_count_;
  };

  explicit OperationBuffer(Zone* zone, size_t initial_capacity) : zone_(zone) {
    begin_ = end_ = zone_->NewArray<OperationStorageSlot>(initial_capacity);
    operation_sizes_ =
        zone_->NewArray<uint16_t>((initial_capacity + 1) / kSlotsPerId);
    end_cap_ = begin_ + initial_capacity;
  }

  OperationStorageSlot* Allocate(size_t slot_count) {
    if (V8_UNLIKELY(static_cast<size_t>(end_cap_ - end_) < slot_count)) {
      Grow(capacity() + slot_count);
      DCHECK(slot_count <= static_cast<size_t>(end_cap_ - end_));
    }
    OperationStorageSlot* result = end_;
    end_ += slot_count;
    OpIndex idx = Index(result);
    // Store the size in both for the first and last id corresponding to the new
    // operation. This enables iteration in both directions. The two id's are
    // the same if the operation is small.
    operation_sizes_[idx.id()] = slot_count;
    operation_sizes_[OpIndex(idx.offset() + static_cast<uint32_t>(slot_count) *
                                                sizeof(OperationStorageSlot))
                         .id() -
                     1] = slot_count;
    return result;
  }

  void RemoveLast() {
    size_t slot_count = operation_sizes_[EndIndex().id() - 1];
    end_ -= slot_count;
    DCHECK_GE(end_, begin_);
  }

  OpIndex Index(const Operation& op) const {
    return Index(reinterpret_cast<const OperationStorageSlot*>(&op));
  }
  OpIndex Index(const OperationStorageSlot* ptr) const {
    DCHECK(begin_ <= ptr && ptr <= end_);
    return OpIndex(static_cast<uint32_t>(reinterpret_cast<Address>(ptr) -
                                         reinterpret_cast<Address>(begin_)));
  }

  OperationStorageSlot* Get(OpIndex idx) {
    DCHECK_LT(idx.offset() / sizeof(OperationStorageSlot), size());
    return reinterpret_cast<OperationStorageSlot*>(
        reinterpret_cast<Address>(begin_) + idx.offset());
  }
  uint16_t SlotCount(OpIndex idx) {
    DCHECK_LT(idx.offset() / sizeof(OperationStorageSlot), size());
    return operation_sizes_[idx.id()];
  }

  const OperationStorageSlot* Get(OpIndex idx) const {
    DCHECK_LT(idx.offset(), capacity() * sizeof(OperationStorageSlot));
    return reinterpret_cast<const OperationStorageSlot*>(
        reinterpret_cast<Address>(begin_) + idx.offset());
  }

  OpIndex Next(OpIndex idx) const {
    DCHECK_GT(operation_sizes_[idx.id()], 0);
    OpIndex result = OpIndex(idx.offset() + operation_sizes_[idx.id()] *
                                                sizeof(OperationStorageSlot));
    DCHECK_LT(0, result.offset());
    DCHECK_LE(result.offset(), capacity() * sizeof(OperationStorageSlot));
    return result;
  }
  OpIndex Previous(OpIndex idx) const {
    DCHECK_GT(idx.id(), 0);
    DCHECK_GT(operation_sizes_[idx.id() - 1], 0);
    OpIndex result = OpIndex(idx.offset() - operation_sizes_[idx.id() - 1] *
                                                sizeof(OperationStorageSlot));
    DCHECK_LE(0, result.offset());
    DCHECK_LT(result.offset(), capacity() * sizeof(OperationStorageSlot));
    return result;
  }

  // Offset of the first operation.
  OpIndex BeginIndex() const { return OpIndex(0); }
  // One-past-the-end offset.
  OpIndex EndIndex() const { return Index(end_); }

  uint32_t size() const { return static_cast<uint32_t>(end_ - begin_); }
  uint32_t capacity() const { return static_cast<uint32_t>(end_cap_ - begin_); }

  void Grow(size_t min_capacity) {
    size_t size = this->size();
    size_t capacity = this->capacity();
    size_t new_capacity = 2 * capacity;
    while (new_capacity < min_capacity) new_capacity *= 2;
    CHECK_LT(new_capacity, std::numeric_limits<uint32_t>::max() /
                               sizeof(OperationStorageSlot));

    OperationStorageSlot* new_buffer =
        zone_->NewArray<OperationStorageSlot>(new_capacity);
    memcpy(new_buffer, begin_, size * sizeof(OperationStorageSlot));

    uint16_t* new_operation_sizes =
        zone_->NewArray<uint16_t>(new_capacity / kSlotsPerId);
    memcpy(new_operation_sizes, operation_sizes_,
           size / kSlotsPerId * sizeof(uint16_t));

    begin_ = new_buffer;
    end_ = new_buffer + size;
    end_cap_ = new_buffer + new_capacity;
    operation_sizes_ = new_operation_sizes;
  }

  void Reset() { end_ = begin_; }

 private:
  Zone* zone_;
  OperationStorageSlot* begin_;
  OperationStorageSlot* end_;
  OperationStorageSlot* end_cap_;
  uint16_t* operation_sizes_;
};

template <class Derived>
class DominatorForwardTreeNode {
  // A class storing a forward representation of the dominator tree, since the
  // regular dominator tree is represented as pointers from the children to
  // parents rather than parents to children.
 public:
  void AddChild(Derived* next) {
    DCHECK_EQ(static_cast<Derived*>(this)->len_ + 1, next->len_);
    next->neighboring_child_ = last_child_;
    last_child_ = next;
  }

  Derived* LastChild() const { return last_child_; }
  Derived* NeighboringChild() const { return neighboring_child_; }
  bool HasChildren() const { return last_child_ != nullptr; }

  base::SmallVector<Derived*, 8> Children() const {
    base::SmallVector<Derived*, 8> result;
    for (Derived* child = last_child_; child != nullptr;
         child = child->neighboring_child_) {
      result.push_back(child);
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

 private:
  friend class Block;

  Derived* neighboring_child_ = nullptr;
  Derived* last_child_ = nullptr;
};

template <class Derived>
class RandomAccessStackDominatorNode
    : public DominatorForwardTreeNode<Derived> {
  // This class represents a node of a dominator tree implemented using Myers'
  // Random-Access Stack (see
  // https://publications.mpi-cbg.de/Myers_1983_6328.pdf). This datastructure
  // enables searching for a predecessor of a node in log(h) time, where h is
  // the height of the dominator tree.
 public:
  void SetDominator(Derived* dominator);
  Derived* GetDominator() { return nxt_; }

  // Returns the lowest common dominator of {this} and {other}.
  Derived* GetCommonDominator(RandomAccessStackDominatorNode<Derived>* other);

  int Depth() const { return len_; }

 private:
  friend class Graph;
  friend class DominatorForwardTreeNode<Derived>;

  int len_ = 0;
  Derived* nxt_ = nullptr;
  Derived* jmp_ = nullptr;
  // Myers' original datastructure requires to often check jmp_->len_, which is
  // not so great on modern computers (memory access, caches & co). To speed up
  // things a bit, we store here jmp_len_.
  int jmp_len_ = 0;
};

// A basic block
class Block : public RandomAccessStackDominatorNode<Block> {
 public:
  enum class Kind : uint8_t { kMerge, kLoopHeader, kBranchTarget };

  bool IsLoopOrMerge() const { return IsLoop() || IsMerge(); }
  bool IsLoop() const { return kind_ == Kind::kLoopHeader; }
  bool IsMerge() const { return kind_ == Kind::kMerge; }
  bool IsHandler() const { return false; }
  bool IsSwitchCase() const { return false; }
  Kind kind() const { return kind_; }

  BlockIndex index() const { return index_; }

  bool IsDeferred() const { return deferred_; }
  void SetDeferred(bool deferred) { deferred_ = deferred; }

  bool Contains(OpIndex op_idx) const {
    return begin_ <= op_idx && op_idx < end_;
  }

  bool IsBound() const { return index_ != BlockIndex::Invalid(); }

  void AddPredecessor(Block* predecessor) {
    DCHECK(!IsBound() ||
           (Predecessors().size() == 1 && kind_ == Kind::kLoopHeader));
    DCHECK_EQ(predecessor->neighboring_predecessor_, nullptr);
    predecessor->neighboring_predecessor_ = last_predecessor_;
    last_predecessor_ = predecessor;
  }

  base::SmallVector<Block*, 8> Predecessors() const {
    base::SmallVector<Block*, 8> result;
    for (Block* pred = last_predecessor_; pred != nullptr;
         pred = pred->neighboring_predecessor_) {
      result.push_back(pred);
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

#ifdef DEBUG
  int PredecessorCount() {
    int count = 0;
    for (Block* pred = last_predecessor_; pred != nullptr;
         pred = pred->neighboring_predecessor_) {
      count++;
    }
    return count;
  }
#endif

  Block* LastPredecessor() const { return last_predecessor_; }
  Block* NeighboringPredecessor() const { return neighboring_predecessor_; }
  bool HasPredecessors() const { return last_predecessor_ != nullptr; }

  // The block from the previous graph which produced the current block. This is
  // used for translating phi nodes from the previous graph.
  void SetOrigin(const Block* origin) {
    DCHECK_NULL(origin_);
    DCHECK_EQ(origin->graph_generation_ + 1, graph_generation_);
    origin_ = origin;
  }
  const Block* Origin() const { return origin_; }

  OpIndex begin() const {
    DCHECK(begin_.valid());
    return begin_;
  }
  OpIndex end() const {
    DCHECK(end_.valid());
    return end_;
  }

  void PrintDominatorTree(
      std::vector<const char*> tree_symbols = std::vector<const char*>(),
      bool has_next = false) const;

  explicit Block(Kind kind) : kind_(kind) {}

 private:
  friend class Graph;

  Kind kind_;
  bool deferred_ = false;
  OpIndex begin_ = OpIndex::Invalid();
  OpIndex end_ = OpIndex::Invalid();
  BlockIndex index_ = BlockIndex::Invalid();
  Block* last_predecessor_ = nullptr;
  Block* neighboring_predecessor_ = nullptr;
  const Block* origin_ = nullptr;
#ifdef DEBUG
  size_t graph_generation_ = 0;
#endif
};

class Graph {
 public:
  // A big initial capacity prevents many growing steps. It also makes sense
  // because the graph and its memory is recycled for following phases.
  explicit Graph(Zone* graph_zone, size_t initial_capacity = 2048)
      : operations_(graph_zone, initial_capacity),
        bound_blocks_(graph_zone),
        all_blocks_(graph_zone),
        graph_zone_(graph_zone),
        source_positions_(graph_zone),
        operation_origins_(graph_zone) {}

  // Reset the graph to recycle its memory.
  void Reset() {
    operations_.Reset();
    bound_blocks_.clear();
    source_positions_.Reset();
    operation_origins_.Reset();
    next_block_ = 0;
  }

  void GenerateDominatorTree();

  const Operation& Get(OpIndex i) const {
    // `Operation` contains const fields and can be overwritten with placement
    // new. Therefore, std::launder is necessary to avoid undefined behavior.
    const Operation* ptr =
        std::launder(reinterpret_cast<const Operation*>(operations_.Get(i)));
    // Detect invalid memory by checking if opcode is valid.
    DCHECK_LT(OpcodeIndex(ptr->opcode), kNumberOfOpcodes);
    return *ptr;
  }
  Operation& Get(OpIndex i) {
    // `Operation` contains const fields and can be overwritten with placement
    // new. Therefore, std::launder is necessary to avoid undefined behavior.
    Operation* ptr =
        std::launder(reinterpret_cast<Operation*>(operations_.Get(i)));
    // Detect invalid memory by checking if opcode is valid.
    DCHECK_LT(OpcodeIndex(ptr->opcode), kNumberOfOpcodes);
    return *ptr;
  }

  const Block& StartBlock() const { return Get(BlockIndex(0)); }

  Block& Get(BlockIndex i) {
    DCHECK_LT(i.id(), bound_blocks_.size());
    return *bound_blocks_[i.id()];
  }
  const Block& Get(BlockIndex i) const {
    DCHECK_LT(i.id(), bound_blocks_.size());
    return *bound_blocks_[i.id()];
  }
  Block* GetPtr(uint32_t index) {
    DCHECK_LT(index, bound_blocks_.size());
    return bound_blocks_[index];
  }

  OpIndex Index(const Operation& op) const { return operations_.Index(op); }

  OperationStorageSlot* Allocate(size_t slot_count) {
    return operations_.Allocate(slot_count);
  }

  void RemoveLast() {
    DecrementInputUses(*AllOperations().rbegin());
    operations_.RemoveLast();
  }

  template <class Op, class... Args>
  V8_INLINE OpIndex Add(Args... args) {
    OpIndex result = next_operation_index();
    Op& op = Op::New(this, args...);
    IncrementInputUses(op);
    DCHECK_EQ(result, Index(op));
#ifdef DEBUG
    for (OpIndex input : op.inputs()) {
      DCHECK_LT(input, result);
    }
#endif  // DEBUG
    return result;
  }

  template <class Op, class... Args>
  void Replace(OpIndex replaced, Args... args) {
    static_assert((std::is_base_of<Operation, Op>::value));
    static_assert(std::is_trivially_destructible<Op>::value);

    const Operation& old_op = Get(replaced);
    DecrementInputUses(old_op);
    auto old_uses = old_op.saturated_use_count;
    Op* new_op;
    {
      OperationBuffer::ReplaceScope replace_scope(&operations_, replaced);
      new_op = &Op::New(this, args...);
    }
    new_op->saturated_use_count = old_uses;
    IncrementInputUses(*new_op);
  }

  V8_INLINE Block* NewBlock(Block::Kind kind) {
    if (V8_UNLIKELY(next_block_ == all_blocks_.size())) {
      constexpr size_t new_block_count = 64;
      base::Vector<Block> blocks =
          graph_zone_->NewVector<Block>(new_block_count, Block(kind));
      for (size_t i = 0; i < new_block_count; ++i) {
        all_blocks_.push_back(&blocks[i]);
      }
    }
    Block* result = all_blocks_[next_block_++];
    *result = Block(kind);
#ifdef DEBUG
    result->graph_generation_ = generation_;
#endif
    return result;
  }

  V8_INLINE bool Add(Block* block) {
    DCHECK_EQ(block->graph_generation_, generation_);
    if (!bound_blocks_.empty() && !block->HasPredecessors()) return false;
    bool deferred = true;
    for (Block* pred = block->last_predecessor_; pred != nullptr;
         pred = pred->neighboring_predecessor_) {
      if (!pred->IsDeferred()) {
        deferred = false;
        break;
      }
    }
    block->SetDeferred(deferred);
    DCHECK(!block->begin_.valid());
    block->begin_ = next_operation_index();
    DCHECK_EQ(block->index_, BlockIndex::Invalid());
    block->index_ = BlockIndex(static_cast<uint32_t>(bound_blocks_.size()));
    bound_blocks_.push_back(block);
    return true;
  }

  void Finalize(Block* block) {
    DCHECK(!block->end_.valid());
    block->end_ = next_operation_index();
  }

  OpIndex next_operation_index() const { return operations_.EndIndex(); }

  Zone* graph_zone() const { return graph_zone_; }
  uint32_t block_count() const {
    return static_cast<uint32_t>(bound_blocks_.size());
  }
  uint32_t op_id_count() const {
    return (operations_.size() + (kSlotsPerId - 1)) / kSlotsPerId;
  }
  uint32_t op_id_capacity() const {
    return operations_.capacity() / kSlotsPerId;
  }

  class OpIndexIterator
      : public base::iterator<std::bidirectional_iterator_tag, OpIndex> {
   public:
    using value_type = OpIndex;

    explicit OpIndexIterator(OpIndex index, const Graph* graph)
        : index_(index), graph_(graph) {}
    value_type& operator*() { return index_; }
    OpIndexIterator& operator++() {
      index_ = graph_->operations_.Next(index_);
      return *this;
    }
    OpIndexIterator& operator--() {
      index_ = graph_->operations_.Previous(index_);
      return *this;
    }
    bool operator!=(OpIndexIterator other) const {
      DCHECK_EQ(graph_, other.graph_);
      return index_ != other.index_;
    }
    bool operator==(OpIndexIterator other) const { return !(*this != other); }

   private:
    OpIndex index_;
    const Graph* const graph_;
  };

  template <class OperationT, typename GraphT>
  class OperationIterator
      : public base::iterator<std::bidirectional_iterator_tag, OperationT> {
   public:
    static_assert(std::is_same_v<std::remove_const_t<OperationT>, Operation> &&
                  std::is_same_v<std::remove_const_t<GraphT>, Graph>);
    using value_type = OperationT;

    explicit OperationIterator(OpIndex index, GraphT* graph)
        : index_(index), graph_(graph) {}
    value_type& operator*() { return graph_->Get(index_); }
    OperationIterator& operator++() {
      index_ = graph_->operations_.Next(index_);
      return *this;
    }
    OperationIterator& operator--() {
      index_ = graph_->operations_.Previous(index_);
      return *this;
    }
    bool operator!=(OperationIterator other) const {
      DCHECK_EQ(graph_, other.graph_);
      return index_ != other.index_;
    }
    bool operator==(OperationIterator other) const { return !(*this != other); }

   private:
    OpIndex index_;
    GraphT* const graph_;
  };

  using MutableOperationIterator = OperationIterator<Operation, Graph>;
  using ConstOperationIterator =
      OperationIterator<const Operation, const Graph>;

  base::iterator_range<MutableOperationIterator> AllOperations() {
    return operations(operations_.BeginIndex(), operations_.EndIndex());
  }
  base::iterator_range<ConstOperationIterator> AllOperations() const {
    return operations(operations_.BeginIndex(), operations_.EndIndex());
  }

  base::iterator_range<OpIndexIterator> AllOperationIndices() const {
    return OperationIndices(operations_.BeginIndex(), operations_.EndIndex());
  }

  base::iterator_range<MutableOperationIterator> operations(
      const Block& block) {
    return operations(block.begin_, block.end_);
  }
  base::iterator_range<ConstOperationIterator> operations(
      const Block& block) const {
    return operations(block.begin_, block.end_);
  }

  base::iterator_range<OpIndexIterator> OperationIndices(
      const Block& block) const {
    return OperationIndices(block.begin_, block.end_);
  }

  base::iterator_range<ConstOperationIterator> operations(OpIndex begin,
                                                          OpIndex end) const {
    DCHECK(begin.valid());
    DCHECK(end.valid());
    return {ConstOperationIterator(begin, this),
            ConstOperationIterator(end, this)};
  }
  base::iterator_range<MutableOperationIterator> operations(OpIndex begin,
                                                            OpIndex end) {
    DCHECK(begin.valid());
    DCHECK(end.valid());
    return {MutableOperationIterator(begin, this),
            MutableOperationIterator(end, this)};
  }

  base::iterator_range<OpIndexIterator> OperationIndices(OpIndex begin,
                                                         OpIndex end) const {
    DCHECK(begin.valid());
    DCHECK(end.valid());
    return {OpIndexIterator(begin, this), OpIndexIterator(end, this)};
  }

  base::iterator_range<base::DerefPtrIterator<Block>> blocks() {
    return {base::DerefPtrIterator<Block>(bound_blocks_.data()),
            base::DerefPtrIterator<Block>(bound_blocks_.data() +
                                          bound_blocks_.size())};
  }
  base::iterator_range<base::DerefPtrIterator<const Block>> blocks() const {
    return {base::DerefPtrIterator<const Block>(bound_blocks_.data()),
            base::DerefPtrIterator<const Block>(bound_blocks_.data() +
                                                bound_blocks_.size())};
  }

  bool IsValid(OpIndex i) const { return i < next_operation_index(); }

  const GrowingSidetable<SourcePosition>& source_positions() const {
    return source_positions_;
  }
  GrowingSidetable<SourcePosition>& source_positions() {
    return source_positions_;
  }

  const GrowingSidetable<OpIndex>& operation_origins() const {
    return operation_origins_;
  }
  GrowingSidetable<OpIndex>& operation_origins() { return operation_origins_; }

  Graph& GetOrCreateCompanion() {
    if (!companion_) {
      companion_ = std::make_unique<Graph>(graph_zone_, operations_.size());
#ifdef DEBUG
      companion_->generation_ = generation_ + 1;
#endif  // DEBUG
    }
    return *companion_;
  }

  // Swap the graph with its companion graph to turn the output of one phase
  // into the input of the next phase.
  void SwapWithCompanion() {
    Graph& companion = GetOrCreateCompanion();
    std::swap(operations_, companion.operations_);
    std::swap(bound_blocks_, companion.bound_blocks_);
    std::swap(all_blocks_, companion.all_blocks_);
    std::swap(next_block_, companion.next_block_);
    std::swap(graph_zone_, companion.graph_zone_);
    std::swap(source_positions_, companion.source_positions_);
    std::swap(operation_origins_, companion.operation_origins_);
#ifdef DEBUG
    // Update generation index.
    DCHECK_EQ(generation_ + 1, companion.generation_);
    generation_ = companion.generation_++;
#endif  // DEBUG
  }

 private:
  bool InputsValid(const Operation& op) const {
    for (OpIndex i : op.inputs()) {
      if (!IsValid(i)) return false;
    }
    return true;
  }

  template <class Op>
  void IncrementInputUses(const Op& op) {
    for (OpIndex input : op.inputs()) {
      Operation& input_op = Get(input);
      auto uses = input_op.saturated_use_count;
      if (V8_LIKELY(uses != Operation::kUnknownUseCount)) {
        input_op.saturated_use_count = uses + 1;
      }
    }
  }

  template <class Op>
  void DecrementInputUses(const Op& op) {
    for (OpIndex input : op.inputs()) {
      Operation& input_op = Get(input);
      auto uses = input_op.saturated_use_count;
      DCHECK_GT(uses, 0);
      // Do not decrement if we already reached the threshold. In this case, we
      // don't know the exact number of uses anymore and shouldn't assume
      // anything.
      if (V8_LIKELY(uses != Operation::kUnknownUseCount)) {
        input_op.saturated_use_count = uses - 1;
      }
    }
  }

  OperationBuffer operations_;
  ZoneVector<Block*> bound_blocks_;
  ZoneVector<Block*> all_blocks_;
  size_t next_block_ = 0;
  Zone* graph_zone_;
  GrowingSidetable<SourcePosition> source_positions_;
  GrowingSidetable<OpIndex> operation_origins_;

  std::unique_ptr<Graph> companion_ = {};
#ifdef DEBUG
  size_t generation_ = 1;
#endif  // DEBUG
};

V8_INLINE OperationStorageSlot* AllocateOpStorage(Graph* graph,
                                                  size_t slot_count) {
  return graph->Allocate(slot_count);
}

struct PrintAsBlockHeader {
  const Block& block;
};
std::ostream& operator<<(std::ostream& os, PrintAsBlockHeader block);
std::ostream& operator<<(std::ostream& os, const Graph& graph);
std::ostream& operator<<(std::ostream& os, const Block::Kind& kind);

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_GRAPH_H_
