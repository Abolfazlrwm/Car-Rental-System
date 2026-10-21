#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <vector>

/**
 * ============================================================
 *  HashTable<K, V>
 * ============================================================
 *  A generic hash map using SEPARATE CHAINING for collision
 *  resolution, with automatic rehashing when the load factor
 *  exceeds 0.75.
 *
 *  Why hand-written?
 *  Provides O(1) average username lookup in the rental system
 *  (the original design used a fixed table of 101 buckets and
 *  silently ignored insertions once buckets filled up).
 *
 *  Complexity (average):
 *      insert / find / remove ... O(1)
 *      rehash ................... O(n), amortized away
 *
 *  The hash function is injectable: by default std::hash<K> is
 *  used, but the RentalManager passes a classic polynomial
 *  rolling hash (base 31) so the hashing strategy itself is
 *  part of the exercise.
 *
 *  Ownership: with ownsValues=true the table deletes every
 *  stored value in clear()/destructor (used for the user index,
 *  where HashTable is the single owner of all User objects).
 * ============================================================
 */
template <typename K, typename V>
class HashTable
{
private:
    struct Node
    {
        K key;
        V value;
        Node *next;

        Node(const K &k, const V &v) : key(k), value(v), next(nullptr) {}
    };

    std::vector<Node *> buckets_;
    std::size_t size_;
    std::function<std::size_t(const K &)> hasher_;
    bool ownsValues_;

    static constexpr double MAX_LOAD_FACTOR = 0.75;

    std::size_t bucketIndex(const K &key) const
    {
        return hasher_(key) % buckets_.size();
    }

    /// Double the bucket count and re-chain every node. O(n)
    void rehash()
    {
        std::size_t newCount = buckets_.size() * 2 + 1;
        std::vector<Node *> newBuckets(newCount, nullptr);

        for (Node *head : buckets_)
        {
            while (head)
            {
                Node *next = head->next;
                std::size_t idx = hasher_(head->key) % newCount;
                head->next = newBuckets[idx];
                newBuckets[idx] = head;
                head = next;
            }
        }
        buckets_.swap(newBuckets);
    }

    void destroyBuckets()
    {
        for (Node *head : buckets_)
        {
            while (head)
            {
                Node *tmp = head;
                head = head->next;
                delete tmp;
            }
        }
        buckets_.assign(buckets_.size(), nullptr);
        size_ = 0;
    }

public:
    explicit HashTable(std::function<std::size_t(const K &)> hasher = std::hash<K>(),
                       bool ownsValues = false)
        : buckets_(17, nullptr), size_(0), hasher_(hasher), ownsValues_(ownsValues) {}

    ~HashTable() { clear(); }

    /// Delete every entry (and the values, when owned). Buckets stay allocated.
    void clear()
    {
        for (Node *head : buckets_)
        {
            while (head)
            {
                Node *tmp = head;
                head = head->next;
                if constexpr (std::is_pointer_v<V>)
                {
                    if (ownsValues_)
                        delete tmp->value;
                }
                delete tmp;
            }
        }
        buckets_.assign(buckets_.size(), nullptr);
        size_ = 0;
    }

    // Non-copyable: the table owns its nodes.
    HashTable(const HashTable &) = delete;
    HashTable &operator=(const HashTable &) = delete;

    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }
    std::size_t bucketCount() const { return buckets_.size(); }
    double loadFactor() const
    {
        return buckets_.empty() ? 0.0 : static_cast<double>(size_) / buckets_.size();
    }

    /// Insert key->value. Returns false (and does nothing) if the key exists.
    bool insert(const K &key, const V &value)
    {
        if (contains(key))
            return false;

        if (loadFactor() > MAX_LOAD_FACTOR)
            rehash();

        std::size_t idx = bucketIndex(key);
        Node *node = new Node(key, value);
        node->next = buckets_[idx];
        buckets_[idx] = node;
        ++size_;
        return true;
    }

    /// Returns nullptr when the key is absent. O(1) average.
    V *find(const K &key)
    {
        std::size_t idx = bucketIndex(key);
        for (Node *curr = buckets_[idx]; curr; curr = curr->next)
            if (curr->key == key)
                return &curr->value;
        return nullptr;
    }

    const V *find(const K &key) const
    {
        return const_cast<HashTable *>(this)->find(key);
    }

    bool contains(const K &key) const { return find(key) != nullptr; }

    /// Remove a key. Returns false when the key was not present.
    bool remove(const K &key)
    {
        std::size_t idx = bucketIndex(key);
        Node *curr = buckets_[idx];
        Node *prev = nullptr;

        while (curr)
        {
            if (curr->key == key)
            {
                if (prev)
                    prev->next = curr->next;
                else
                    buckets_[idx] = curr->next;
                delete curr;
                --size_;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    /// Visit every (key, value) pair in arbitrary order.
    void forEach(const std::function<void(const K &, const V &)> &fn) const
    {
        for (Node *head : buckets_)
            for (Node *curr = head; curr; curr = curr->next)
                fn(curr->key, curr->value);
    }

    /// Number of buckets whose chain is non-empty (diagnostics / tests).
    std::size_t usedBuckets() const
    {
        std::size_t used = 0;
        for (Node *head : buckets_)
            if (head)
                ++used;
        return used;
    }
};

#endif // HASH_TABLE_H
