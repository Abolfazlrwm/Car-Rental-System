#include "AVLTree.h"

AVLTree::AVLTree() : root(nullptr) {}
AVLTree::~AVLTree() { destroy(root); }

int manualMax(int a, int b)
{
    return (a > b) ? a : b;
}

/* ===== Utility ===== */
int AVLTree::height(AVLNode *node) const
{
    return node ? node->getHeight() : 0;
}

int AVLTree::balanceFactor(AVLNode *node) const
{
    return node ? height(node->getLeft()) - height(node->getRight()) : 0;
}

/* ===== Rotations ===== */
AVLNode *AVLTree::rotateRight(AVLNode *y)
{
    AVLNode *x = y->getLeft();
    AVLNode *T2 = x->getRight();

    x->setRight(y);
    y->setLeft(T2);

    y->setHeight(std::max(height(y->getLeft()), height(y->getRight())) + 1);
    x->setHeight(std::max(height(x->getLeft()), height(x->getRight())) + 1);

    return x;
}

AVLNode *AVLTree::rotateLeft(AVLNode *x)
{
    AVLNode *y = x->getRight();
    AVLNode *T2 = y->getLeft();

    y->setLeft(x);
    x->setRight(T2);

    x->setHeight(manualMax(height(x->getLeft()), height(x->getRight())) + 1);
    y->setHeight(manualMax(height(y->getLeft()), height(y->getRight())) + 1);

    return y;
}

/* ===== Insert ===== */
AVLNode *AVLTree::insert(AVLNode *node, const std::string &key, Car *value)
{
    if (!node)
        return new AVLNode(key, value);

    if (key < node->getKey())
        node->setLeft(insert(node->getLeft(), key, value));
    else if (key > node->getKey())
        node->setRight(insert(node->getRight(), key, value));
    else
        return node; // duplicate keys not allowed

    node->setHeight(std::max(height(node->getLeft()), height(node->getRight())) + 1);
    int balance = balanceFactor(node);

    // LL
    if (balance > 1 && key < node->getLeft()->getKey())
        return rotateRight(node);

    // RR
    if (balance < -1 && key > node->getRight()->getKey())
        return rotateLeft(node);

    // LR
    if (balance > 1 && key > node->getLeft()->getKey())
    {
        node->setLeft(rotateLeft(node->getLeft()));
        return rotateRight(node);
    }

    // RL
    if (balance < -1 && key < node->getRight()->getKey())
    {
        node->setRight(rotateRight(node->getRight()));
        return rotateLeft(node);
    }

    return node;
}

void AVLTree::insert(const std::string &key, Car *value)
{
    root = insert(root, key, value);
}

/* ===== Find ===== */
AVLNode *AVLTree::find(AVLNode *node, const std::string &key) const
{
    if (!node)
        return nullptr;

    if (key == node->getKey())
        return node;

    if (key < node->getKey())
        return find(node->getLeft(), key);

    return find(node->getRight(), key);
}

Car *AVLTree::find(const std::string &key) const
{
    AVLNode *node = find(root, key);
    return node ? node->getValue() : nullptr;
}

/* ===== Destroy ===== */
void AVLTree::destroy(AVLNode *node)
{
    if (!node)
        return;
    destroy(node->getLeft());
    destroy(node->getRight());
    delete node;
}