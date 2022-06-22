# Coding style

* use the latest C++ (C++23/26 currently)
* snake_case
* `struct` instead of `class`
* `typename` instead of `class` in templates
* almost always `auto &&`
* braces
```
something {
}
```
* write code in headers as must as possible
* use {} style initialization where possible
```
type var{args...};
```
* write pointers and references right to variable, not type
```
void *var; //ok
int &v; // ok
int& x; // wrong
```
