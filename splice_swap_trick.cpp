// how to use list::splice to swap two its
#include <list>
#include<iostream>
using namespace std;

template <typename T>

void print_list(list<T> v) {
  for (auto i : v) {
    cout << i << " ";
  }
  cout << endl;
}

int main() { 
    list<long> l;
    l.push_back(0);
    l.push_back(1);
    l.push_back(2);
    l.push_back(3);
    l.push_back(4);
    l.push_back(5);
    l.push_back(6);
    print_list<long>(l);
    auto it1 = next(l.begin(),2);
    auto it2 = next(l.begin(),6);
    auto it_temp = next(it2);
    cout << *it1 << " " << *it2 << endl;
    l.splice(next(it1), l, it2);
    l.splice(it_temp, l, it1);
    print_list<long>(l);
    return 0;
}