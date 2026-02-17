#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <string>
#include "AVLNode.h"
#include "../manager_system/Car.h"

class AVLTree
{
private:
    AVLNode *root;

    int height(AVLNode *node) const;
    int balanceFactor(AVLNode *node) const;

    AVLNode *rotateRight(AVLNode *y);
    AVLNode *rotateLeft(AVLNode *x);

    AVLNode *insert(AVLNode *node, const string &key, Car *value);
    AVLNode *find(AVLNode *node, const string &key) const;

    void destroy(AVLNode *node);

public:
    AVLTree();
    ~AVLTree();

    void insert(const string &key, Car *value);
    Car *find(const string &key) const;
};

#endif
