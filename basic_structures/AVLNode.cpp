#include "AVLNode.h"

AVLNode::AVLNode(const string &key, Car *value)
    : key(key), value(value), left(nullptr), right(nullptr), height(1) {}

// Getters
string AVLNode::getKey() const { return key; }
Car *AVLNode::getValue() const { return value; }
AVLNode *AVLNode::getLeft() const { return left; }
AVLNode *AVLNode::getRight() const { return right; }
int AVLNode::getHeight() const { return height; }

// Setters
void AVLNode::setLeft(AVLNode *node) { left = node; }
void AVLNode::setRight(AVLNode *node) { right = node; }
void AVLNode::setHeight(int h) { height = h; }
