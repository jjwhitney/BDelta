/* Copyright (C) 2003-2010  John Whitney
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: John Whitney <jjw@deltup.org>
 */
 
/*
struct Stack_node {
	Stack_node(void *datA, Stack_node *nexT) {data = datA; next = nexT;}
	void *data; 
	Stack_node *next;
};

template <class T>
class Stack {
	Stack_node *s;
public:
	Stack() {s = 0;}
	void push(T* t) {s = new Stack_node(t, s);}
	void pop() {Stack_node *old = s; s = s->next; delete old;}
	T& top() {return *((T*)s->data);}
	bool empty() {return (s==0);}
	void clear() {while (!empty()) pop();}
};
*/

template <class T>
struct DLink {
	T *obj;
	DLink *prev, *next;
	DLink(T *obJ, DLink *preV, DLink *nexT) : obj(obJ), prev(preV), next(nexT) {
		if (preV) preV->next = this;
		if (nexT) nexT->prev = this;
	}
	void erase() {
		if (prev) prev->next = next;
		if (next) next->prev = prev;
		delete this;
	}
};

template <class T>
struct DList {
	DLink<T> *first, *last;
	DList() : first(0), last(0) {}
	DLink<T> *find_first(T *o) {
		for (DLink<T> *i = first; i; i=i->next)
			if (i->obj==o) return i;
		return 0;
	}

	int size() {int j = 0; for (DLink<T> *i = first; i; i=i->next) ++j; return j;}
	DLink<T> *insert(T *o, DLink<T> *prev, DLink<T> *next);
	void push_front(T *o) {insert(o, 0, first);}
	void push_back(T *o) {insert(o, last, 0);}
	void erase(DLink<T> *o);
	bool empty() {return !first;}
};

template <class T>
DLink<T> *DList<T>::insert(T *o, DLink<T> *prev, DLink<T> *next) {
	DLink<T> *newobj = new DLink<T>(o, prev, next);
	if (prev == last) last = newobj;
	if (next == first) first = newobj;
	return newobj;
}

template <class T>
void DList<T>::erase(DLink<T> *obj) {
	if (obj==first) first=obj->next;
	if (obj==last) last=obj->prev;
	obj->erase();
}
