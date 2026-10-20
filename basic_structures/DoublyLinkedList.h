#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

#include <cstddef>
#include <functional>
#include <sstream>
#include <stdexcept>

/**
 * ============================================================
 *  DoublyLinkedList<T>
 * ============================================================
 *  A generic doubly linked list with O(1) push at both ends.
 *
 *  Why hand-written instead of std::list?
 *  This project is a data-structures exercise: every core
 *  container is implemented from scratch to demonstrate the
 *  underlying pointer mechanics.
 *
 *  Complexity:
 *      pushFront / pushBack / popFront / popBack ... O(1)
 *      insertAt / removeAt / getAt / indexOf ....... O(n)
 *      forEach ..................................... O(n)
 *
 *  Note: nodes are owned by the list and freed in clear().
 *  Copying is disabled to keep single-ownership semantics
 *  explicit (use forEach to read elements).
 * ============================================================
 */
template <typename T>
class DoublyLinkedList
{
private:
    struct Node
    {
        T data;
        Node *next;
        Node *prev;

        explicit Node(const T &value) : data(value), next(nullptr), prev(nullptr) {}
    };

    Node *head_;
    Node *tail_;
    std::size_t size_;

public:
    DoublyLinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~DoublyLinkedList() { clear(); }

    // Non-copyable: the list owns its nodes.
    DoublyLinkedList(const DoublyLinkedList &) = delete;
    DoublyLinkedList &operator=(const DoublyLinkedList &) = delete;

    /* ---------------- Capacity ---------------- */

    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }

    /* ---------------- Modifiers ---------------- */

    void pushFront(const T &value)
    {
        Node *node = new Node(value);
        if (!head_)
        {
            head_ = tail_ = node;
        }
        else
        {
            node->next = head_;
            head_->prev = node;
            head_ = node;
        }
        ++size_;
    }

    void pushBack(const T &value)
    {
        Node *node = new Node(value);
        if (!tail_)
        {
            head_ = tail_ = node;
        }
        else
        {
            node->prev = tail_;
            tail_->next = node;
            tail_ = node;
        }
        ++size_;
    }

    /// Insert at position [0 .. size()]. Returns false if out of range.
    bool insertAt(std::size_t pos, const T &value)
    {
        if (pos > size_)
            return false;
        if (pos == 0)
        {
            pushFront(value);
            return true;
        }
        if (pos == size_)
        {
            pushBack(value);
            return true;
        }

        // Walk from the nearer end for fewer hops.
        Node *curr;
        if (pos < size_ / 2)
        {
            curr = head_;
            for (std::size_t i = 0; i < pos; ++i)
                curr = curr->next;
        }
        else
        {
            curr = tail_;
            for (std::size_t i = size_ - 1; i > pos; --i)
                curr = curr->prev;
        }

        Node *node = new Node(value);
        node->prev = curr->prev;
        node->next = curr;
        curr->prev->next = node;
        curr->prev = node;
        ++size_;
        return true;
    }

    bool popFront()
    {
        if (!head_)
            return false;
        Node *tmp = head_;
        head_ = head_->next;
        if (head_)
            head_->prev = nullptr;
        else
            tail_ = nullptr;
        delete tmp;
        --size_;
        return true;
    }

    bool popBack()
    {
        if (!tail_)
            return false;
        Node *tmp = tail_;
        tail_ = tail_->prev;
        if (tail_)
            tail_->next = nullptr;
        else
            head_ = nullptr;
        delete tmp;
        --size_;
        return true;
    }

    /// Remove the node at position [0 .. size()-1]. Returns false if out of range.
    bool removeAt(std::size_t pos)
    {
        if (pos >= size_)
            return false;
        if (pos == 0)
            return popFront();
        if (pos == size_ - 1)
            return popBack();

        Node *curr;
        if (pos < size_ / 2)
        {
            curr = head_;
            for (std::size_t i = 0; i < pos; ++i)
                curr = curr->next;
        }
        else
        {
            curr = tail_;
            for (std::size_t i = size_ - 1; i > pos; --i)
                curr = curr->prev;
        }

        curr->prev->next = curr->next;
        curr->next->prev = curr->prev;
        delete curr;
        --size_;
        return true;
    }

    /// Remove the first occurrence of `value`. Returns false if not found.
    bool removeValue(const T &value)
    {
        int idx = indexOf(value);
        if (idx < 0)
            return false;
        return removeAt(static_cast<std::size_t>(idx));
    }

    void clear()
    {
        while (head_)
        {
            Node *tmp = head_;
            head_ = head_->next;
            delete tmp;
        }
        tail_ = nullptr;
        size_ = 0;
    }

    /* ---------------- Access & search ---------------- */

    /// Returns the first index of `value`, or -1 if not found. O(n)
    int indexOf(const T &value) const
    {
        int idx = 0;
        for (Node *curr = head_; curr; curr = curr->next, ++idx)
            if (curr->data == value)
                return idx;
        return -1;
    }

    bool contains(const T &value) const { return indexOf(value) >= 0; }

    /// Bounds-checked element access (throws std::out_of_range).
    T &getAt(std::size_t pos)
    {
        if (pos >= size_)
            throw std::out_of_range("DoublyLinkedList::getAt: position out of range");
        Node *curr;
        if (pos < size_ / 2)
        {
            curr = head_;
            for (std::size_t i = 0; i < pos; ++i)
                curr = curr->next;
        }
        else
        {
            curr = tail_;
            for (std::size_t i = size_ - 1; i > pos; --i)
                curr = curr->prev;
        }
        return curr->data;
    }

    const T &getAt(std::size_t pos) const
    {
        // Reuse the non-const version (no mutation happens here).
        return const_cast<DoublyLinkedList *>(this)->getAt(pos);
    }

    const T &front() const
    {
        if (!head_)
            throw std::out_of_range("DoublyLinkedList::front: list is empty");
        return head_->data;
    }

    const T &back() const
    {
        if (!tail_)
            throw std::out_of_range("DoublyLinkedList::back: list is empty");
        return tail_->data;
    }

    /// Iterate over all elements in insertion order (head -> tail).
    void forEach(const std::function<void(const T &)> &fn) const
    {
        for (Node *curr = head_; curr; curr = curr->next)
            fn(curr->data);
    }

    /// Human-readable dump, e.g. "[a, b, c]". Used by the unit tests.
    std::string toString() const
    {
        std::ostringstream oss;
        oss << '[';
        bool first = true;
        forEach([&](const T &v)
                {
                    if (!first)
                        oss << ", ";
                    oss << v;
                    first = false;
                });
        oss << ']';
        return oss.str();
    }
};

#endif // DOUBLY_LINKED_LIST_H
