#ifndef NODE_H
#define NODE_H

using namespace std;
#include <string>

class Node
{
public:
    string data;
    Node *next;
    Node *prev;

    Node(const string &value);
};

#endif