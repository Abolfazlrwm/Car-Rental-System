#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <vector>

/**
 * ============================================================
 *  AVLTree<K, V>
 * ============================================================
 *  A self-balancing binary search tree (Adelson-Velsky &
 *  Landis). Every node stores a key/value pair; after each
 *  insertion the subtree heights are re-checked and one of the
 *  four classic rotations (LL, RR, LR, RL) restores a balance
 *  factor of at most 1.
 *
 *  Why hand-written?
 *  Indexes the car fleet by name so lookups stay O(log n) even
 *  if the fleet is inserted in already-sorted order — a plain
 *  BST would degrade to a linked list in that case.
 *
 *  Complexity:
 *      insert / find / contains ... O(log n) guaranteed
 *      inOrderForEach ............. O(n), keys in sorted order
 *      height ..................... O(1) (root height stored)
 *
 *  The tree owns neither keys nor values unless constructed
 *  with ownsValues=true — see ownership notes in RentalManager.
 * ============================================================
 */
template <typename K, typename V>
class AVLTree
{
private:
    struct Node
    {
        K key;
        V value;
        Node *left;
        Node *right;
        int height;

        Node(const K &k, const V &v)
            : key(k), value(v), left(nullptr), right(nullptr), height(1) {}
    };

    Node *root_;
    std::size_t size_;
    bool ownsValues_;

    /* ---------------- Height & balance ---------------- */

    static int nodeHeight(Node *n) { return n ? n->height : 0; }

    /// balance = height(left) - height(right); AVL requires |balance| <= 1.
    static int balanceFactor(Node *n)
    {
        return n ? nodeHeight(n->left) - nodeHeight(n->right) : 0;
    }

    static void updateHeight(Node *n)
    {
        n->height = 1 + std::max(nodeHeight(n->left), nodeHeight(n->right));
    }

    /* ---------------- Rotations ----------------

         rotateRight(y):            rotateLeft(x):

              y                          x
             / \                        / \
            x   T3        ==>          T1   y
           / \                             / \
          T1  T2                          T2  T3
    */
    Node *rotateRight(Node *y)
    {
        Node *x = y->left;
        Node *t2 = x->right;

        x->right = y;
        y->left = t2;

        updateHeight(y);
        updateHeight(x);
        return x;
    }

    /*   rotateLeft(x):

           x                    y
          / \                  / \
         T1  y      ==>       x   T3
            / \              / \
           T2  T3           T1  T2
    */
    Node *rotateLeft(Node *x)
    {
        Node *y = x->right;
        Node *t2 = y->left;

        y->left = x;
        x->right = t2;

        updateHeight(x);
        updateHeight(y);
        return y;
    }

    /// Recursive insert with rebalancing (returns the new subtree root).
    Node *insert(Node *node, const K &key, const V &value)
    {
        if (!node)
        {
            ++size_;
            return new Node(key, value);
        }

        if (key < node->key)
            node->left = insert(node->left, key, value);
        else if (node->key < key)
            node->right = insert(node->right, key, value);
        else
            return node; // duplicate key -> keep the original value

        updateHeight(node);
        int balance = balanceFactor(node);

        // Left-Left case: single right rotation.
        if (balance > 1 && key < node->left->key)
            return rotateRight(node);

        // Right-Right case: single left rotation.
        if (balance < -1 && node->right->key < key)
            return rotateLeft(node);

        // Left-Right case: rotate left child leftwards, then right.
        if (balance > 1 && node->left->key < key)
        {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }

        // Right-Left case: rotate right child rightwards, then left.
        if (balance < -1 && key < node->right->key)
        {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }

        return node;
    }

    static Node *findNode(Node *node, const K &key)
    {
        while (node)
        {
            if (key < node->key)
                node = node->left;
            else if (node->key < key)
                node = node->right;
            else
                return node;
        }
        return nullptr;
    }

    static void destroy(Node *node, bool ownsValues)
    {
        if (!node)
            return;
        destroy(node->left, ownsValues);
        destroy(node->right, ownsValues);
        if constexpr (std::is_pointer_v<V>)
        {
            if (ownsValues)
                delete node->value;
        }
        delete node;
    }

public:
    explicit AVLTree(bool ownsValues = false)
        : root_(nullptr), size_(0), ownsValues_(ownsValues) {}

    ~AVLTree() { destroy(root_, ownsValues_); }

    /// Delete every node (and owned values), resetting the tree.
    void clear()
    {
        destroy(root_, ownsValues_);
        root_ = nullptr;
        size_ = 0;
    }

    // Non-copyable: keeps ownership reasoning simple.
    AVLTree(const AVLTree &) = delete;
    AVLTree &operator=(const AVLTree &) = delete;

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    int height() const { return nodeHeight(root_); }

    void insert(const K &key, const V &value)
    {
        root_ = insert(root_, key, value);
    }

    /// nullptr when the key is absent. O(log n).
    V *find(const K &key)
    {
        Node *n = findNode(root_, key);
        return n ? &n->value : nullptr;
    }

    const V *find(const K &key) const
    {
        return const_cast<AVLTree *>(this)->find(key);
    }

    bool contains(const K &key) const { return find(key) != nullptr; }

    /// Visit every value with keys in ASCENDING order (in-order traversal).
    void inOrderForEach(const std::function<void(const K &, const V &)> &fn) const
    {
        // Iterative in-order walk with an explicit stack of visited nodes.
        // (Recursive lambdas are awkward; this keeps the traversal tidy.)
        std::vector<Node *> stack;
        Node *curr = root_;

        while (curr || !stack.empty())
        {
            while (curr)
            {
                stack.push_back(curr);
                curr = curr->left;
            }
            curr = stack.back();
            stack.pop_back();
            fn(curr->key, curr->value);
            curr = curr->right;
        }
    }

    /// Theoretical height bound check helper (used in the unit tests).
    /// For an AVL tree with n nodes: height <= 1.4405 * log2(n+2) - 0.3277.
    static double maxHeightBound(std::size_t n)
    {
        if (n < 2)
            return 1.0;
        return 1.4405 * (std::log2(static_cast<double>(n) + 2.0)) - 0.3277;
    }
};

#endif // AVL_TREE_H
