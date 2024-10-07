#pragma once

#include <algorithm>
#include <alloca.h>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

template <typename T>
concept Fundamental = std::is_fundamental_v<T>;

template <Fundamental T = char, class Allocator = std::allocator<T>>
class GapBuffer {
public:
    // STL Compatible Container types
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = std::allocator_traits<Allocator>::pointer;
    using const_pointer = std::allocator_traits<Allocator>::const_pointer;

private:
    // Iterator to abstract the gap
    template <typename PointerType>
    class GapIterator {
    public:
        // std::conditional<condition, typeIfTrue, typeIfFalse>
        // All necessary for using adapters (std::reverse_iterator<it>) + STL
        // compatibilty
        static const bool is_const =
            std::is_const_v<std::remove_pointer_t<PointerType>>;
        using iterator_category = std::contiguous_iterator_tag;
        using gapbuffer_pointer =
            std::conditional<is_const, const GapBuffer*, GapBuffer*>::type;

        using value_type =
            std::conditional<is_const, const GapBuffer::value_type,
                             GapBuffer::value_type>::type;
        using element_type =
            std::conditional<is_const, const value_type, value_type>::type;

        using pointer = PointerType;
        using reference = value_type&;
        using difference_type =
            std::ptrdiff_t; // difference type of 2 iterators
        // difference between INDEXES NOT values

        gapbuffer_pointer gb;
        PointerType ptr;

        explicit GapIterator() = default;
        // self = GapBuffer that will be iterated
        explicit GapIterator(gapbuffer_pointer self, PointerType input_ptr)
            : gb(self), ptr(input_ptr) {
        }

        GapIterator& operator++() {
            ptr += 1;
            if (ptr == gb->gapStart) {
                ptr = gb->gapEnd;
            }
            return *this;
        }

        GapIterator operator++(int) {
            GapIterator tmp = *this;
            ++(*this);
            if (ptr == gb->gapStart) {
                ptr = gb->gapEnd;
            }
            return *this;
        }

        GapIterator& operator--() {
            ptr -= 1;
            if (ptr == gb->gapEnd) {
                ptr = gb->gapStart - 1;
            }
            return *this;
        }

        GapIterator operator--(int) {
            GapIterator tmp = *this;
            --(*this);
            if (ptr == gb->gapEnd) {
                ptr = gb->gapStart - 1;
            }
            return tmp;
        }

        // provides functionality for it + value
        GapIterator operator+(const int val) {
            GapIterator tmp = *this;
            return tmp += val;
        }

        GapIterator operator+(const difference_type other) const {
            return GapBuffer(gb, ptr + other);
        }

        // provides functionality for value + it
        friend GapIterator operator+(const difference_type val,
                                     const GapIterator& other) {
            return other + val;
        }

        GapIterator operator-(const int val) {
            GapIterator tmp = *this;
            return tmp -= val;
        }

        // difference between iterators !! accounting for gap
        difference_type operator-(const GapIterator& other) const {
            // if both before gap
            if (ptr < gb->gapStart && other.ptr < gb->gapStart) {
                return ptr - other.ptr;
            }
            // if both after gap
            else if (ptr > gb->gapEnd && other.ptr > gb->gapEnd) {
                return ptr - other.ptr;
            }
            // if on opposite sides of gap
            // total distance WITHOUT the gap
            else {
                return (gb->gapStart - other.ptr) + (ptr - gb->gapEnd);
            }
        }

        GapIterator operator-(const difference_type other) const {
            return GapIterator(gb, ptr - other);
        }

        friend GapIterator operator-(const difference_type value,
                                     const GapIterator& other) {
            return other - value;
        }

        GapIterator& operator+=(difference_type val) {
            ptr += val;
            return *this;
        }

        GapIterator& operator-=(difference_type val) {
            ptr -= val;
            return *this;
        }

        reference operator*() {
            return *ptr;
        }
        PointerType operator->() const {
            return &operator*();
        }
    };

    // Iterator member types
    using iterator = GapIterator<pointer>;
    using const_iterator = GapIterator<const_pointer>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    // https://en.cppreference.com/w/cpp/memory/uninitialized_copy_n
    // allocated memory is uninitialized, cannot use std::copy yet
    constexpr explicit GapBuffer() {
        bufferStart = allocator_type().allocate(32);
        bufferEnd = std::uninitialized_value_construct_n(bufferStart, 32);
        gapStart = bufferStart;
        gapEnd = bufferEnd;
    }

    constexpr explicit GapBuffer(const size_type& size) {
        bufferStart = allocator_type().allocate(size);
        bufferEnd = std::uninitialized_value_construct_n(bufferStart, size);
        gapStart = bufferStart;
        gapEnd = bufferEnd;
    }

    constexpr explicit GapBuffer(std::string_view str) {
        bufferStart = allocator_type().allocate(str.size() + 8);
        std::uninitialized_copy_n(str.begin(), str.size(), bufferStart);

        gapStart = bufferStart + str.size();
        gapEnd = gapStart + 8;
        bufferEnd = gapEnd;
    }

    template <typename It>
    constexpr explicit GapBuffer(It start, It end) {
        const size_type len = std::distance(start, end);

        bufferStart =
            allocator_type().allocate(len + 8); // range + gap (length 8)
        std::uninitialized_copy_n(start, len, bufferStart);

        gapStart = bufferStart + len;
        gapEnd = gapStart + 8;
        bufferEnd = gapEnd;
    }

    // Copy Constructor
    // new instance = copy of other instance
    constexpr GapBuffer(const GapBuffer& other) {
        bufferStart = allocator_type().allocate(other.capacity());
        std::uninitialized_copy_n(other.bufferStart, other.capacity(),
                                  bufferStart);

        gapStart =
            bufferStart +
            (other.gapStart - other.bufferStart); // subtracting bufferStart
                                                  // necessary for the offset
        gapEnd = bufferStart + (other.gapEnd - other.bufferStart); // same here
        bufferEnd = bufferStart + other.capacity();
    }

    // Copy Assignment
    // initialized instance = copy of other instance
    constexpr GapBuffer& operator=(const GapBuffer& other) {
        if (this != &other) {
            pointer newBuffStart = allocator_type().allocate(other.capacity());
            std::uninitialized_copy_n(other.bufferStart, other.capacity(),
                                      newBuffStart);

            std::destroy_n(bufferStart, capacity());
            allocator_type().deallocate(bufferStart, capacity());

            bufferStart = newBuffStart;
            gapStart = newBuffStart + (other.gapStart - other.bufferStart);
            gapEnd = newBuffStart + (other.gapEnd - other.bufferStart);
            bufferEnd = newBuffStart + other.capacity();
        }

        return *this;
    }

    // Move Constructor
    // new instance = std::move(other instance)
    constexpr GapBuffer(GapBuffer&& other) noexcept
        : bufferStart(other.bufferStart), gapStart(other.gapStart),
          gapEnd(other.gapEnd), bufferEnd(other.bufferEnd) {
        other.bufferStart = nullptr;
        other.gapStart = nullptr;
        other.gapEnd = nullptr;
        other.bufferEnd = nullptr;
    }

    // Move Assignment
    // initialized instance = std::move(other instance)
    constexpr GapBuffer& operator=(GapBuffer&& other) noexcept {
        if (this != &other) {
            std::destroy_n(bufferStart, capacity());
            allocator_type().deallocate(bufferStart, capacity());

            bufferStart = other.bufferStart;
            gapStart = other.gapStart;
            gapEnd = other.gapEnd;
            bufferEnd = other.bufferEnd;

            other.bufferStart = nullptr;
            other.gapStart = nullptr;
            other.gapEnd = nullptr;
            other.bufferEnd = nullptr;
        }

        return *this;
    }

    ~GapBuffer() noexcept {
        std::destroy_n(bufferStart, capacity());
        allocator_type().deallocate(bufferStart, capacity());
    }

    constexpr reference at(const size_type pos) {
        if (pos >= size()) {
            throw std::out_of_range("Out of bounds");
        }
        const size_type gapStartIndex =
            static_cast<size_type>(gapStart - bufferStart);
        if (pos < gapStartIndex) {
            return *(bufferStart + pos);
        } else if (pos > *gapStart) {
            return *(gapEnd + (pos - (gapStart - bufferStart)));
        } else {
            throw std::out_of_range("Pos cannot be in gap");
        }
    }

    constexpr reference at(const size_type pos) const {
        if (pos >= size()) {
            throw std::out_of_range("Out of bounds");
        }
        const size_type gapStartIndex =
            static_cast<size_type>(gapStart - bufferStart);
        if (pos < gapStartIndex) {
            return *(bufferStart + pos);
        } else if (pos > *gapStart) {
            return *(gapEnd + (pos - (gapStart - bufferStart)));
        } else {
            throw std::out_of_range("Pos cannot be in gap");
        }
    }

    iterator begin() noexcept {
        return iterator(this, bufferStart);
    }

    const_iterator begin() const noexcept {
        return const_iterator(this, bufferStart);
    }

    const_iterator cbegin() const noexcept {
        return const_iterator(this, bufferStart);
    }

    iterator end() noexcept {
        return iterator(this, bufferEnd);
    }

    const_iterator end() const noexcept {
        return const_iterator(this, bufferEnd);
    }

    const_iterator cend() const noexcept {
        return const_iterator(this, bufferEnd);
    }

    // reverse iterators
    reverse_iterator rbegin() noexcept {
        return reverse_iterator(
            iterator(this, (gapEnd == bufferEnd) ? gapStart : bufferEnd));
    }

    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(
            iterator(this, (gapEnd == bufferEnd) ? gapStart : bufferEnd));
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(
            iterator(this, (gapStart == bufferStart) ? gapEnd : bufferStart));
    }

    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(const_iterator(
            this, (gapStart == bufferStart) ? gapEnd : bufferStart));
    }

    // size ignoring the gap
    constexpr size_type size() {
        return bufferEnd - bufferStart - (gapEnd - gapStart);
    }

    constexpr size_type size() const {
        return bufferEnd - bufferStart - (gapEnd - gapStart);
    }

    constexpr size_type gapSize() {
        return static_cast<size_type>(gapEnd - gapStart);
    }

    constexpr size_type gapSize() const {
        return static_cast<size_type>(gapEnd - gapStart);
    }

    constexpr size_type capacity() {
        return bufferEnd - bufferStart;
    }

    constexpr size_type capacity() const {
        return bufferEnd - bufferStart;
    }

    constexpr void clear() noexcept {
        std::destroy_n(bufferStart, capacity());
        gapStart = bufferStart;
        gapEnd = bufferEnd;
    }

    constexpr void resize(const size_type count) {
        if (count <= size()) {
            return;
        }

        pointer newMem =
            allocator_type().allocate(count); // acts as newBufferStart

        size_type prefixSize = gapStart - bufferStart;
        size_type suffixSize = bufferEnd - gapEnd;
        std::uninitialized_copy_n(bufferStart, prefixSize, newMem);
        std::uninitialized_copy_n(gapEnd, suffixSize,
                                  newMem + prefixSize + (count - capacity()));

        allocator_type().deallocate(bufferStart, capacity());

        gapStart = newMem + prefixSize;
        gapEnd = gapStart + (count - capacity());
        bufferStart = newMem;
        bufferEnd = newMem + count;
    }

    // cannot be string_view because sv locally dangles
    constexpr std::string to_string() {
        std::string ret;
        ret.reserve(size()); // reserve size of buffer w/o gap for performance
        ret.append(bufferStart, (gapStart - bufferStart));
        ret.append(gapEnd, (bufferEnd - gapEnd));
        return ret;
    }

    constexpr void insert(iterator pos, const_reference value) {
        if (gapSize() <= 1) {
            int posLen = pos - begin();
            resize(capacity() * 2);
            pos = begin() + posLen + 1;
        }

        std::uninitialized_copy_n(pos.ptr, gapStart - pos.ptr,
                                  pos.ptr + 1); // move [pos, end) over 1
        *pos.ptr = value;
        gapStart++;
    }

    constexpr void insert(iterator pos, std::string_view str) {
        if (gapSize() <= str.size()) {
            int posLen = pos - begin();
            resize(capacity() * 2);
            pos = begin() + posLen + 1;
        }

        std::uninitialized_copy_n(
            pos.ptr, gapStart - pos.ptr,
            pos.ptr + str.size()); // move [pos, end) over length of str
        std::copy(str.begin(), str.end(),
                  pos.ptr); // copy string to space of length str made
        gapStart += str.size();
    }

    constexpr void erase(iterator pos) {
        std::copy(pos.ptr + 1, gapStart, pos.ptr);
        --gapStart;
    }

    constexpr void erase(iterator pos, const size_type count) {
        std::copy(pos.ptr + count, gapStart, pos.ptr);
        gapStart -= count;
    }

    constexpr void push_back(const T& value) {
        if (gapStart == gapEnd) {
            resize(capacity() * 2);
        }
        *gapStart = value;
        gapStart++;
    }

private:
    // raw pointers like a "raw iterator"
    // support arithmetic and everything but unsafe and less functionality
    pointer bufferStart = nullptr;
    pointer gapStart = nullptr;
    pointer gapEnd = nullptr;
    pointer bufferEnd = nullptr;
};
