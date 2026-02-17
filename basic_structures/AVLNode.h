#ifndef AVL_NODE_H
#define AVL_NODE_H

using namespace std;
#include <string>
#include "../manager_system/Car.h"

class AVLNode
{
private:
    string key;
    Car *value;
    AVLNode *left;
    AVLNode *right;
    int height;

public:
    AVLNode(const string &key, Car *value);

    // Getters
    string getKey() const;
    Car *getValue() const;
    AVLNode *getLeft() const;
    AVLNode *getRight() const;
    int getHeight() const;

    // Setters
    void setLeft(AVLNode *node);
    void setRight(AVLNode *node);
    void setHeight(int h);
};

#endif
