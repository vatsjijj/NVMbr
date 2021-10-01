#include <stdlib.h>
#include <string.h>
#include "include/memory.h"
#include "include/object.h"
#include "include/table.h"
#include "include/value.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void free_table(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  init_table(table);
}

static Entry* find_entry(Entry* entries, int capacity, ObjString* key) {
  uint32_t index = key->hash & (capacity - 1);
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];

    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        return tombstone != NULL ? tombstone : entry;
      }
      else {
        if (tombstone == NULL) tombstone = entry;
      }
    }
    else if (entry->key == key) {
      return entry;
    }
    index = (index + 1) & (capacity - 1);
  }
}

bool get_table(Table* table, ObjString* key, Value* value) {
  if (table->count == 0) return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  *value = entry->value;

  return true;
}

static void adj_capacity(Table* table, int capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);

  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;

  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];

    if (entry->key == NULL) continue;

    Entry* dest = find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;

    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);

  table->entries = entries;
  table->capacity = capacity;
}

bool set_table(Table* table, ObjString* key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adj_capacity(table, capacity);
  }

  Entry* entry = find_entry(table->entries, table->capacity, key);

  bool is_new_key = entry->key == NULL;
  if (is_new_key && IS_NIL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;

  return is_new_key;
}

bool del_table(Table* table, ObjString* key) {
  if (table->count == 0) return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  entry->key = NULL;
  entry->value = BOOL_VAL(true);

  return true;
}

void table_add_all(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];

    if (entry->key != NULL) {
      set_table(to, entry->key, entry->value);
    }
  }
}

ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash & (table->capacity - 1);

  for (;;) {
    Entry* entry = &table->entries[index];

    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) return NULL;
    }
    else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
      return entry->key;
    }
    index = (index + 1) & (table->capacity - 1);
  }
}

void table_rmwhi(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];

    if (entry->key != NULL && !entry->key->obj.is_marked) {
      del_table(table, entry->key);
    }
  }
}

void mark_table(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    
    mark_obj((Obj*)entry->key);
    mark_val(entry->value);
  }
}
