/*****************************************************************************
 * ElasticHashMap.hpp (or inline in your file)
 *****************************************************************************/
#pragma once
#include <functional>
#include <vector>
#include <optional>
#include <cassert>
#include <stdexcept>
#include <cmath>    // for floor, etc.

/**
 * ElasticHashMap:
 * A templated open-addressing hash table (multiple sub-arrays) that
 * avoids reordering items once placed. Keys and Values are stored in
 * "slots" within each subarray. If a subarray is too full (or can't find
 * a slot quickly), we skip to the next subarray.
 *
 * Template parameters:
 *   K = key type
 *   V = mapped/aggregated value type
 *   Hasher = functor for hashing a key
 */
template<typename K, typename V, typename Hasher = std::hash<K>>
class ElasticHashMap {
public:
    // -----------------------------
    // Subarray struct
    // -----------------------------
    struct Slot {
        bool occupied;
        K key;
        V value;

        Slot() : occupied(false) {}
        Slot(const K& k, const V& v) : occupied(true), key(k), value(v) {}
    };

    struct Subarray {
        std::vector<Slot> slots;
        size_t size;       // number of items currently stored
        size_t capacity;   // total slots

        Subarray(size_t cap) : size(0), capacity(cap) {
            slots.resize(cap);
        }
    };

public:
    /**
     * Constructor: Provide a total capacity and a hasher.
     * We'll create ~log(totalCap) subarrays with geometric sizes, but you
     * can tune them as you prefer. For example, if totalCap = 1024, we might
     * do subarray sizes of 512, 256, 128, ...
     */
    ElasticHashMap(size_t totalCapacity, Hasher hasher = Hasher())
        : m_hasher(hasher)
    {
        if (totalCapacity < 8) {
            totalCapacity = 8; // minimum
        }

        // We'll build subarrays with descending sizes until we sum to totalCapacity.
        // A simpler approach is to split by 1/2 each time. For example:
        //   A1 = totalCapacity/2, A2 = totalCapacity/4, etc.
        // We'll do this in a loop:
        size_t remaining = totalCapacity;
        while (remaining > 0) {
            size_t subCap = std::max<size_t>(1, remaining / 2);
            if (subCap < 4 && remaining < 4) {
                subCap = remaining; // final subarray
            }
            subarrays.emplace_back(subCap);
            remaining -= subCap;
        }
    }

    /**
     * Check existence of a key (like unordered_map.count(key))
     */
    bool count(const K& key) {
        return (findSlot(key) != nullptr);
    }

    /**
     * Return a reference to the value for `key`, creating a default if not found
     * (similar to std::unordered_map::operator[]).
     *
     * NOTE: If you prefer a "find(key)->pointer" approach, see findSlot(key).
     */
    V& operator[](const K& key) {
        // find or insert a default
        auto slotPtr = findSlot(key);
        if (slotPtr) {
            return slotPtr->value;  // found
        }
        // else insert a default
        static V defaultVal{}; // T must have a default ctor
        insertOrUpdate(key, defaultVal);
        // now find again
        slotPtr = findSlot(key);
        if (!slotPtr) {
            throw std::runtime_error("ElasticHashMap: insertion unexpectedly failed!");
        }
        return slotPtr->value;
    }

    /**
     * Insert or update the key with value. If the key exists, we overwrite.
     */
    void insertOrUpdate(const K& key, const V& val) {
        if (tryUpdate(key, val)) {
            // Key existed, we updated
            return;
        }
        // Key didn't exist, so do an insert
        doInsert(key, val);
    }

public:
    // The subarrays
    std::vector<Subarray> subarrays;
    Hasher m_hasher;

private:
    /**
     * Try updating an existing slot. Return true if updated, false if not found.
     */
    bool tryUpdate(const K& key, const V& val) {
        // We'll attempt to look in each subarray i in order
        for (auto& sub : subarrays) {
            size_t subcap = sub.capacity;
            // We'll do a short linear or quadratic search
            size_t hashVal = m_hasher(key);
            // For demonstration, let's do a small # of attempts in sub i:
            // The paper might do more sophisticated logic, but we'll keep it simple:
            size_t maxProbes = std::min<size_t>(16, subcap); // up to 16 or capacity
            for (size_t probe = 0; probe < maxProbes; probe++) {
                size_t idx = (hashVal + probe) % subcap;
                if (sub.slots[idx].occupied) {
                    if (sub.slots[idx].key == key) {
                        // found key => update
                        sub.slots[idx].value = val;
                        return true;
                    }
                } else {
                    // empty slot => key can't be here
                    break; 
                }
            }
            // if not found in subarray i, continue to next subarray
        }
        return false;
    }

    /**
     * Actually insert a brand-new key. Return void. If we can't find space,
     * you could throw an exception or do a "final fallback subarray," etc.
     */
    void doInsert(const K& key, const V& val) {
        // We'll do the logic: for each subarray i, try up to a limited # of probes:
        for (auto& sub : subarrays) {
            // If sub is <, say, 90% full, we allow searching in sub
            if (sub.size * 10 < sub.capacity * 9) {
                size_t subcap = sub.capacity;
                size_t hashVal = m_hasher(key);
                size_t maxProbes = std::min<size_t>(16, subcap);
                for (size_t probe = 0; probe < maxProbes; probe++) {
                    size_t idx = (hashVal + probe) % subcap;
                    if (!sub.slots[idx].occupied) {
                        // place it
                        sub.slots[idx].occupied = true;
                        sub.slots[idx].key = key;
                        sub.slots[idx].value = val;
                        sub.size++;
                        return;
                    }
                    else if (sub.slots[idx].key == key) {
                        // means it was inserted concurrently or found a duplicate
                        sub.slots[idx].value = val;
                        return;
                    }
                }
                // If we can't find an empty slot within maxProbes, we skip to next subarray
            }
        }
        // If we get here, no subarray could store it => no space or too many collisions
        throw std::runtime_error("ElasticHashMap: table is too full or insertion failed!");
    }

    /**
     * Return a pointer to the slot if found, else null.
     */
    Slot* findSlot(const K& key) {
        // same searching approach as tryUpdate
        for (auto& sub : subarrays) {
            size_t subcap = sub.capacity;
            size_t hashVal = m_hasher(key);
            size_t maxProbes = std::min<size_t>(16, subcap);
            for (size_t probe = 0; probe < maxProbes; probe++) {
                size_t idx = (hashVal + probe) % subcap;
                if (!sub.slots[idx].occupied) {
                    // once we see an empty, the chain is done
                    break;
                }
                if (sub.slots[idx].occupied && sub.slots[idx].key == key) {
                    return &sub.slots[idx];
                }
            }
        }
        return nullptr;
    }
};

