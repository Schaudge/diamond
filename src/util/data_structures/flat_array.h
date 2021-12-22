/****
DIAMOND protein aligner
Copyright (C) 2020-2021 Max Planck Society for the Advancement of Science e.V.

Code developed by Benjamin Buchfink <benjamin.buchfink@tue.mpg.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#pragma once
#include <vector>

template<typename T>
struct FlatArray {

	typedef typename std::vector<T>::iterator DataIterator;
	typedef typename std::vector<T>::const_iterator DataConstIterator;

	FlatArray() {
		limits_.push_back(0);
	}

	void push_back(const T &x) {
		data_.push_back(x);
		++limits_.back();
	}

	void push_back(DataConstIterator begin, DataConstIterator end) {
		data_.insert(data_.end(), begin, end);
		limits_.push_back(limits_.back() + (end - begin));
	}

	void next() {
		limits_.push_back(limits_.back());
	}

	void pop_back() {
		limits_.pop_back();
	}

	void clear() {
		data_.clear();
		limits_.clear();
		limits_.push_back(0);
	}

	size_t size() const {
		return limits_.size() - 1;
	}

	size_t data_size() const {
		return data_.size();
	}

	DataConstIterator begin(size_t i) const {
		return data_.cbegin() + limits_[i];
	}

	DataConstIterator end(size_t i) const {
		return data_.cbegin() + limits_[i + 1];
	}

	DataConstIterator cbegin(size_t i) const {
		return data_.cbegin() + limits_[i];
	}

	DataConstIterator cend(size_t i) const {
		return data_.cbegin() + limits_[i + 1];
	}

	DataIterator begin(size_t i) {
		return data_.begin() + limits_[i];
	}

	DataIterator end(size_t i) {
		return data_.begin() + limits_[i + 1];
	}

	int64_t count(size_t i) const {
		return limits_[i + 1] - limits_[i];
	}

	struct ConstIterator {
		ConstIterator(typename std::vector<size_t>::const_iterator limits, typename std::vector<T>::const_iterator data_begin):
			limits_(limits),
			data_begin_(data_begin)
		{}
		DataConstIterator begin(size_t i) const {
			return data_begin_ + limits_[i];
		}
		DataConstIterator end(size_t i) const {
			return data_begin_ + limits_[i + 1];
		}
		int64_t operator-(const ConstIterator& i) const {
			return limits_ - i.limits_;
		}
	private:
		typename std::vector<size_t>::const_iterator limits_;
		typename std::vector<T>::const_iterator data_begin_;
	};

	ConstIterator cbegin() const {
		return ConstIterator(limits_.cbegin(), data_.cbegin());
	}

	ConstIterator cend() const {
		return ConstIterator(limits_.cend() - 1, data_.cbegin());
	}

	struct Iterator {
		Iterator() {}
		Iterator(typename std::vector<size_t>::const_iterator limits, typename std::vector<T>::iterator data_begin) :
			limits_(limits),
			data_begin_(data_begin)
		{}
		DataIterator begin(size_t i) const {
			return data_begin_ + limits_[i];
		}
		DataIterator end(size_t i) const {
			return data_begin_ + limits_[i + 1];
		}
		int64_t operator-(const Iterator& i) const {
			return limits_ - i.limits_;
		}
	private:
		typename std::vector<size_t>::const_iterator limits_;
		typename std::vector<T>::iterator data_begin_;
	};

	Iterator begin() {
		return Iterator(limits_.cbegin(), data_.begin());
	}

	Iterator end() {
		return Iterator(limits_.cend() - 1, data_.begin());
	}

	void reserve(const int64_t size, const int64_t data_size) {
		data_.reserve(data_size);
		limits_.reserve(size + 1);
	}

private:

	std::vector<T> data_;
	std::vector<size_t> limits_;

};