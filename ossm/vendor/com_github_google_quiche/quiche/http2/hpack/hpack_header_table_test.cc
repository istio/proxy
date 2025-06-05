// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/hpack/hpack_header_table.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/http2/hpack/hpack_constants.h"
#include "quiche/http2/hpack/hpack_entry.h"
#include "quiche/http2/hpack/hpack_static_table.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace spdy {

using std::distance;

namespace test {

class HpackHeaderTablePeer {
 public:
  explicit HpackHeaderTablePeer(HpackHeaderTable* table) : table_(table) {}

  const HpackHeaderTable::DynamicEntryTable& dynamic_entries() {
    return table_->dynamic_entries_;
  }
  const HpackHeaderTable::StaticEntryTable& static_entries() {
    return table_->static_entries_;
  }
  const HpackEntry* GetFirstStaticEntry() {
    return &table_->static_entries_.front();
  }
  const HpackEntry* GetLastStaticEntry() {
    return &table_->static_entries_.back();
  }
  std::vector<HpackEntry*> EvictionSet(absl::string_view name,
                                       absl::string_view value) {
    HpackHeaderTable::DynamicEntryTable::iterator begin, end;
    table_->EvictionSet(name, value, &begin, &end);
    std::vector<HpackEntry*> result;
    for (; begin != end; ++begin) {
      result.push_back(begin->get());
    }
    return result;
  }
  size_t dynamic_table_insertions() {
    return table_->dynamic_table_insertions_;
  }
  size_t EvictionCountForEntry(absl::string_view name,
                               absl::string_view value) {
    return table_->EvictionCountForEntry(name, value);
  }
  size_t EvictionCountToReclaim(size_t reclaim_size) {
    return table_->EvictionCountToReclaim(reclaim_size);
  }
  void Evict(size_t count) { return table_->Evict(count); }

 private:
  HpackHeaderTable* table_;
};

}  // namespace test

namespace {

class HpackHeaderTableTest : public quiche::test::QuicheTest {
 protected:
  typedef std::vector<HpackEntry> HpackEntryVector;

  HpackHeaderTableTest() : table_(), peer_(&table_) {}

  // Returns an entry whose Size() is equal to the given one.
  static HpackEntry MakeEntryOfSize(uint32_t size) {
    EXPECT_GE(size, kHpackEntrySizeOverhead);
    std::string name((size - kHpackEntrySizeOverhead) / 2, 'n');
    std::string value(size - kHpackEntrySizeOverhead - name.size(), 'v');
    HpackEntry entry(name, value);
    EXPECT_EQ(size, entry.Size());
    return entry;
  }

  // Returns a vector of entries whose total size is equal to the given
  // one.
  static HpackEntryVector MakeEntriesOfTotalSize(uint32_t total_size) {
    EXPECT_GE(total_size, kHpackEntrySizeOverhead);
    uint32_t entry_size = kHpackEntrySizeOverhead;
    uint32_t remaining_size = total_size;
    HpackEntryVector entries;
    while (remaining_size > 0) {
      EXPECT_LE(entry_size, remaining_size);
      entries.push_back(MakeEntryOfSize(entry_size));
      remaining_size -= entry_size;
      entry_size = std::min(remaining_size, entry_size + 32);
    }
    return entries;
  }

  // Adds the given vector of entries to the given header table,
  // expecting no eviction to happen.
  void AddEntriesExpectNoEviction(const HpackEntryVector& entries) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
      HpackHeaderTable::DynamicEntryTable::iterator begin, end;

      table_.EvictionSet(it->name(), it->value(), &begin, &end);
      EXPECT_EQ(0, distance(begin, end));

      const HpackEntry* entry = table_.TryAddEntry(it->name(), it->value());
      EXPECT_NE(entry, static_cast<HpackEntry*>(nullptr));
    }
  }

  HpackHeaderTable table_;
  test::HpackHeaderTablePeer peer_;
};

TEST_F(HpackHeaderTableTest, StaticTableInitialization) {
  EXPECT_EQ(0u, table_.size());
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, table_.max_size());
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, table_.settings_size_bound());

  EXPECT_EQ(0u, peer_.dynamic_entries().size());
  EXPECT_EQ(0u, peer_.dynamic_table_insertions());

  // Static entries have been populated and inserted into the table & index.
  const HpackHeaderTable::StaticEntryTable& static_entries =
      peer_.static_entries();
  EXPECT_EQ(kStaticTableSize, static_entries.size());
  // HPACK indexing scheme is 1-based.
  size_t index = 1;
  for (const HpackEntry& entry : static_entries) {
    EXPECT_EQ(index, table_.GetByNameAndValue(entry.name(), entry.value()));
    index++;
  }
}

TEST_F(HpackHeaderTableTest, BasicDynamicEntryInsertionAndEviction) {
  EXPECT_EQ(kStaticTableSize, peer_.static_entries().size());

  const HpackEntry* first_static_entry = peer_.GetFirstStaticEntry();
  const HpackEntry* last_static_entry = peer_.GetLastStaticEntry();

  const HpackEntry* entry = table_.TryAddEntry("header-key", "Header Value");
  EXPECT_EQ("header-key", entry->name());
  EXPECT_EQ("Header Value", entry->value());

  // Table counts were updated appropriately.
  EXPECT_EQ(entry->Size(), table_.size());
  EXPECT_EQ(1u, peer_.dynamic_entries().size());
  EXPECT_EQ(kStaticTableSize, peer_.static_entries().size());

  EXPECT_EQ(62u, table_.GetByNameAndValue("header-key", "Header Value"));

  // Index of static entries does not change.
  EXPECT_EQ(first_static_entry, peer_.GetFirstStaticEntry());
  EXPECT_EQ(last_static_entry, peer_.GetLastStaticEntry());

  // Evict |entry|. Table counts are again updated appropriately.
  peer_.Evict(1);
  EXPECT_EQ(0u, table_.size());
  EXPECT_EQ(0u, peer_.dynamic_entries().size());
  EXPECT_EQ(kStaticTableSize, peer_.static_entries().size());

  // Index of static entries does not change.
  EXPECT_EQ(first_static_entry, peer_.GetFirstStaticEntry());
  EXPECT_EQ(last_static_entry, peer_.GetLastStaticEntry());
}

TEST_F(HpackHeaderTableTest, EntryIndexing) {
  const HpackEntry* first_static_entry = peer_.GetFirstStaticEntry();
  const HpackEntry* last_static_entry = peer_.GetLastStaticEntry();

  // Static entries are queryable by name & value.
  EXPECT_EQ(1u, table_.GetByName(first_static_entry->name()));
  EXPECT_EQ(1u, table_.GetByNameAndValue(first_static_entry->name(),
                                         first_static_entry->value()));

  // Create a mix of entries which duplicate names, and names & values of both
  // dynamic and static entries.
  table_.TryAddEntry(first_static_entry->name(), first_static_entry->value());
  table_.TryAddEntry(first_static_entry->name(), "Value Four");
  table_.TryAddEntry("key-1", "Value One");
  table_.TryAddEntry("key-2", "Value Three");
  table_.TryAddEntry("key-1", "Value Two");
  table_.TryAddEntry("key-2", "Value Three");
  table_.TryAddEntry("key-2", "Value Four");

  // The following entry is identical to the one at index 68.  The smaller index
  // is returned by GetByNameAndValue().
  EXPECT_EQ(1u, table_.GetByNameAndValue(first_static_entry->name(),
                                         first_static_entry->value()));
  EXPECT_EQ(67u,
            table_.GetByNameAndValue(first_static_entry->name(), "Value Four"));
  EXPECT_EQ(66u, table_.GetByNameAndValue("key-1", "Value One"));
  EXPECT_EQ(64u, table_.GetByNameAndValue("key-1", "Value Two"));
  // The following entry is identical to the one at index 65.  The smaller index
  // is returned by GetByNameAndValue().
  EXPECT_EQ(63u, table_.GetByNameAndValue("key-2", "Value Three"));
  EXPECT_EQ(62u, table_.GetByNameAndValue("key-2", "Value Four"));

  // Index of static entries does not change.
  EXPECT_EQ(first_static_entry, peer_.GetFirstStaticEntry());
  EXPECT_EQ(last_static_entry, peer_.GetLastStaticEntry());

  // Querying by name returns the most recently added matching entry.
  EXPECT_EQ(64u, table_.GetByName("key-1"));
  EXPECT_EQ(62u, table_.GetByName("key-2"));
  EXPECT_EQ(1u, table_.GetByName(first_static_entry->name()));
  EXPECT_EQ(kHpackEntryNotFound, table_.GetByName("not-present"));

  // Querying by name & value returns the lowest-index matching entry among
  // static entries, and the highest-index one among dynamic entries.
  EXPECT_EQ(66u, table_.GetByNameAndValue("key-1", "Value One"));
  EXPECT_EQ(64u, table_.GetByNameAndValue("key-1", "Value Two"));
  EXPECT_EQ(63u, table_.GetByNameAndValue("key-2", "Value Three"));
  EXPECT_EQ(62u, table_.GetByNameAndValue("key-2", "Value Four"));
  EXPECT_EQ(1u, table_.GetByNameAndValue(first_static_entry->name(),
                                         first_static_entry->value()));
  EXPECT_EQ(67u,
            table_.GetByNameAndValue(first_static_entry->name(), "Value Four"));
  EXPECT_EQ(kHpackEntryNotFound,
            table_.GetByNameAndValue("key-1", "Not Present"));
  EXPECT_EQ(kHpackEntryNotFound,
            table_.GetByNameAndValue("not-present", "Value One"));

  // Evict |entry1|. Queries for its name & value now return the static entry.
  // |entry2| remains queryable.
  peer_.Evict(1);
  EXPECT_EQ(1u, table_.GetByNameAndValue(first_static_entry->name(),
                                         first_static_entry->value()));
  EXPECT_EQ(67u,
            table_.GetByNameAndValue(first_static_entry->name(), "Value Four"));

  // Evict |entry2|. Queries by its name & value are not found.
  peer_.Evict(1);
  EXPECT_EQ(kHpackEntryNotFound,
            table_.GetByNameAndValue(first_static_entry->name(), "Value Four"));

  // Index of static entries does not change.
  EXPECT_EQ(first_static_entry, peer_.GetFirstStaticEntry());
  EXPECT_EQ(last_static_entry, peer_.GetLastStaticEntry());
}

TEST_F(HpackHeaderTableTest, SetSizes) {
  std::string key = "key", value = "value";
  const HpackEntry* entry1 = table_.TryAddEntry(key, value);
  const HpackEntry* entry2 = table_.TryAddEntry(key, value);
  const HpackEntry* entry3 = table_.TryAddEntry(key, value);

  // Set exactly large enough. No Evictions.
  size_t max_size = entry1->Size() + entry2->Size() + entry3->Size();
  table_.SetMaxSize(max_size);
  EXPECT_EQ(3u, peer_.dynamic_entries().size());

  // Set just too small. One eviction.
  max_size = entry1->Size() + entry2->Size() + entry3->Size() - 1;
  table_.SetMaxSize(max_size);
  EXPECT_EQ(2u, peer_.dynamic_entries().size());

  // Changing SETTINGS_HEADER_TABLE_SIZE.
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, table_.settings_size_bound());
  // In production, the size passed to SetSettingsHeaderTableSize is never
  // larger than table_.settings_size_bound().
  table_.SetSettingsHeaderTableSize(kDefaultHeaderTableSizeSetting * 3 + 1);
  EXPECT_EQ(kDefaultHeaderTableSizeSetting * 3 + 1, table_.max_size());

  // SETTINGS_HEADER_TABLE_SIZE upper-bounds |table_.max_size()|,
  // and will force evictions.
  max_size = entry3->Size() - 1;
  table_.SetSettingsHeaderTableSize(max_size);
  EXPECT_EQ(max_size, table_.max_size());
  EXPECT_EQ(max_size, table_.settings_size_bound());
  EXPECT_EQ(0u, peer_.dynamic_entries().size());
}

TEST_F(HpackHeaderTableTest, EvictionCountForEntry) {
  std::string key = "key", value = "value";
  const HpackEntry* entry1 = table_.TryAddEntry(key, value);
  const HpackEntry* entry2 = table_.TryAddEntry(key, value);
  size_t entry3_size = HpackEntry::Size(key, value);

  // Just enough capacity for third entry.
  table_.SetMaxSize(entry1->Size() + entry2->Size() + entry3_size);
  EXPECT_EQ(0u, peer_.EvictionCountForEntry(key, value));
  EXPECT_EQ(1u, peer_.EvictionCountForEntry(key, value + "x"));

  // No extra capacity. Third entry would force evictions.
  table_.SetMaxSize(entry1->Size() + entry2->Size());
  EXPECT_EQ(1u, peer_.EvictionCountForEntry(key, value));
  EXPECT_EQ(2u, peer_.EvictionCountForEntry(key, value + "x"));
}

TEST_F(HpackHeaderTableTest, EvictionCountToReclaim) {
  std::string key = "key", value = "value";
  const HpackEntry* entry1 = table_.TryAddEntry(key, value);
  const HpackEntry* entry2 = table_.TryAddEntry(key, value);

  EXPECT_EQ(1u, peer_.EvictionCountToReclaim(1));
  EXPECT_EQ(1u, peer_.EvictionCountToReclaim(entry1->Size()));
  EXPECT_EQ(2u, peer_.EvictionCountToReclaim(entry1->Size() + 1));
  EXPECT_EQ(2u, peer_.EvictionCountToReclaim(entry1->Size() + entry2->Size()));
}

// Fill a header table with entries. Make sure the entries are in
// reverse order in the header table.
TEST_F(HpackHeaderTableTest, TryAddEntryBasic) {
  EXPECT_EQ(0u, table_.size());
  EXPECT_EQ(table_.settings_size_bound(), table_.max_size());

  HpackEntryVector entries = MakeEntriesOfTotalSize(table_.max_size());

  // Most of the checks are in AddEntriesExpectNoEviction().
  AddEntriesExpectNoEviction(entries);
  EXPECT_EQ(table_.max_size(), table_.size());
  EXPECT_EQ(table_.settings_size_bound(), table_.size());
}

// Fill a header table with entries, and then ramp the table's max
// size down to evict an entry one at a time. Make sure the eviction
// happens as expected.
TEST_F(HpackHeaderTableTest, SetMaxSize) {
  HpackEntryVector entries =
      MakeEntriesOfTotalSize(kDefaultHeaderTableSizeSetting / 2);
  AddEntriesExpectNoEviction(entries);

  for (auto it = entries.begin(); it != entries.end(); ++it) {
    size_t expected_count = distance(it, entries.end());
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());

    table_.SetMaxSize(table_.size() + 1);
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());

    table_.SetMaxSize(table_.size());
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());

    --expected_count;
    table_.SetMaxSize(table_.size() - 1);
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());
  }
  EXPECT_EQ(0u, table_.size());
}

// Fill a header table with entries, and then add an entry just big
// enough to cause eviction of all but one entry. Make sure the
// eviction happens as expected and the long entry is inserted into
// the table.
TEST_F(HpackHeaderTableTest, TryAddEntryEviction) {
  HpackEntryVector entries = MakeEntriesOfTotalSize(table_.max_size());
  AddEntriesExpectNoEviction(entries);

  // The first entry in the dynamic table.
  const HpackEntry* survivor_entry = peer_.dynamic_entries().front().get();

  HpackEntry long_entry =
      MakeEntryOfSize(table_.max_size() - survivor_entry->Size());

  // All dynamic entries but the first are to be evicted.
  EXPECT_EQ(peer_.dynamic_entries().size() - 1,
            peer_.EvictionSet(long_entry.name(), long_entry.value()).size());

  table_.TryAddEntry(long_entry.name(), long_entry.value());
  EXPECT_EQ(2u, peer_.dynamic_entries().size());
  EXPECT_EQ(63u, table_.GetByNameAndValue(survivor_entry->name(),
                                          survivor_entry->value()));
  EXPECT_EQ(62u,
            table_.GetByNameAndValue(long_entry.name(), long_entry.value()));
}

// Fill a header table with entries, and then add an entry bigger than
// the entire table. Make sure no entry remains in the table.
TEST_F(HpackHeaderTableTest, TryAddTooLargeEntry) {
  HpackEntryVector entries = MakeEntriesOfTotalSize(table_.max_size());
  AddEntriesExpectNoEviction(entries);

  const HpackEntry long_entry = MakeEntryOfSize(table_.max_size() + 1);

  // All entries are to be evicted.
  EXPECT_EQ(peer_.dynamic_entries().size(),
            peer_.EvictionSet(long_entry.name(), long_entry.value()).size());

  const HpackEntry* new_entry =
      table_.TryAddEntry(long_entry.name(), long_entry.value());
  EXPECT_EQ(new_entry, static_cast<HpackEntry*>(nullptr));
  EXPECT_EQ(0u, peer_.dynamic_entries().size());
}

}  // namespace

}  // namespace spdy
