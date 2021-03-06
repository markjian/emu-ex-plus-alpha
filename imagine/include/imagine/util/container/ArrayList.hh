#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include "containerUtils.hh"
#include <assert.h>
#include <cstddef>
#include <iterator>
#include <algorithm>
#include <cstring>

template <class T, size_t SIZE>
struct StaticStorageBase
{
	constexpr StaticStorageBase() {}
	T arr[SIZE]{};
	T *storage() { return arr; }
	const T *storage() const { return arr; }
	constexpr size_t maxSize() const { return SIZE; }
};

template <class T>
struct PointerStorageBase
{
	constexpr PointerStorageBase() {}
	T *arr{};
	T *storage() { return arr; }
	const T *storage() const { return arr; }

	size_t size = 0;
	void setStorage(T *s, size_t size) { arr = s; this->size = size;}
	size_t maxSize() const { return size; }
};

template<class T, class STORAGE_BASE>
class ArrayListBase : public STORAGE_BASE
{
private:
	size_t size_ = 0;

public:
	using STORAGE_BASE::storage;
	using value_type = T;
	using iterator = T*;
	using const_iterator = const T*;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr ArrayListBase() {}

	bool remove(const T &val)
	{
		return IG::removeFirst(*this, val);
	}

	// Iterators (STL API)
	iterator begin() { return data(); }
	iterator end() { return data() + size(); }
	const_iterator begin() const { return data(); }
	const_iterator end() const { return data() + size(); }
	const_iterator cbegin() const { return begin(); }
	const_iterator cend() const { return end(); }
	reverse_iterator rbegin() { return reverse_iterator(end()); }
	reverse_iterator rend() { return reverse_iterator(begin()); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
	const_reverse_iterator crbegin() const { return rbegin(); }
	const_reverse_iterator crend() const { return rend(); }

	// Capacity (STL API)
	size_t size() const { return size_; }
	bool empty() const { return !size(); };
	size_t capacity() const { return STORAGE_BASE::maxSize(); }
	size_t max_size() const { return STORAGE_BASE::maxSize(); }

	void resize(size_t size)
	{
		assert(size <= max_size());
		size_= size;
	}

	// Capacity
	bool isFull() const
	{
		return !freeSpace();
	}

	size_t freeSpace() const
	{
		return capacity() - size();
	}

	// Element Access (STL API)
	T &front() { return at(0);	}
	T &back() { return at(size()-1);	}

	T &at(size_t idx)
	{
		assert(idx < size());
		return (*this)[idx];
	}

	T *data() { return storage(); }
	const T *data() const { return storage(); }

	T& operator[] (int idx) { return data()[idx]; }
	const T& operator[] (int idx) const { return data()[idx]; }

	// Modifiers (STL API)
	void clear()
	{
		//logMsg("removing all array list items (%d)", size_);
		size_ = 0;
	}

	void pop_back()
	{
		assert(size_);
		size_--;
	}

	void push_back(const T &d)
	{
		assert(size_ < max_size());
		data()[size_] = d;
		size_++;
	}

	template <class... ARGS>
	T &emplace_back(ARGS&&... args)
	{
		assert(size_ < max_size());
		auto newAddr = &data()[size_];
		new(newAddr) T(std::forward<ARGS>(args)...);
		size_++;
		return *newAddr;
	}

	iterator insert(const_iterator position, const T& val)
	{
		// TODO: re-write using std::move
		uintptr_t idx = position - data();
		assert(idx <= size());
		uintptr_t elemsAfterInsertIdx = size()-idx;
		if(elemsAfterInsertIdx)
		{
			std::memmove(&data()[idx+1], &data()[idx], sizeof(T)*elemsAfterInsertIdx);
		}
		data()[idx] = val;
		size_++;
		return &data()[idx];
	}

	iterator erase(iterator position)
	{
		if(position + 1 != end())
			std::move(position + 1, end(), position);
		size_--;
		return position;
	}

	iterator erase(iterator first, iterator last)
	{
		if(first != last)
		{
			if(last != end())
				std::move(last, end(), first);
			size_ -= std::distance(first, last);
		}
		return first;
	}
};


template<class T, size_t SIZE>
using StaticArrayList = ArrayListBase<T, StaticStorageBase<T, SIZE> >;
