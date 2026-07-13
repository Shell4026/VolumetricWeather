// GPT5.6으로 작성
#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>

/// @brief 원소가 꽉차면 head위치에 새로 삽입하는 배열 기반 원형 큐
template <typename T, std::size_t Capacity>
class CircularQueue
{
    static_assert(Capacity > 0, "Capacity must be greater than zero.");

public:
    [[nodiscard]]
    constexpr bool Empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]]
    constexpr bool Full() const noexcept
    {
        return size_ == Capacity;
    }

    [[nodiscard]]
    constexpr std::size_t Size() const noexcept
    {
        return size_;
    }

    [[nodiscard]]
    static constexpr std::size_t MaxSize() noexcept
    {
        return Capacity;
    }

    // 큐 뒤에 삽입한다.
    // 가득 찬 경우 가장 오래된 원소를 덮어쓴다.
    void Push(const T& value)
    {
        Insert(value);
    }

    void Push(T&& value)
    {
        Insert(std::move(value));
    }

    template <typename... Args>
    T& Emplace(Args&&... args)
    {
        const std::size_t index = GetInsertionIndex();

        data_[index] = T(std::forward<Args>(args)...);

        UpdateAfterInsertion();

        return data_[index];
    }

    // 가장 오래된 원소 제거
    bool Pop()
    {
        if (Empty())
            return false;

        head_ = NextIndex(head_);
        --size_;

        return true;
    }

    // 가장 오래된 원소
    T& Front()
    {
        if (Empty())
            throw std::out_of_range("CircularQueue is empty.");

        return data_[head_];
    }

    const T& Front() const
    {
        if (Empty())
            throw std::out_of_range("CircularQueue is empty.");

        return data_[head_];
    }

    // 가장 최근에 삽입된 원소
    T& Back()
    {
        if (Empty())
            throw std::out_of_range("CircularQueue is empty.");

        return data_[(head_ + size_ - 1) % Capacity];
    }

    const T& Back() const
    {
        if (Empty())
            throw std::out_of_range("CircularQueue is empty.");

        return data_[(head_ + size_ - 1) % Capacity];
    }

    // 오래된 원소부터 접근
    T& operator[](std::size_t index)
    {
        if (index >= size_)
            throw std::out_of_range("CircularQueue index out of range.");

        return data_[(head_ + index) % Capacity];
    }

    const T& operator[](std::size_t index) const
    {
        if (index >= size_)
            throw std::out_of_range("CircularQueue index out of range.");

        return data_[(head_ + index) % Capacity];
    }

    void Clear() noexcept
    {
        head_ = 0;
        size_ = 0;
    }

private:
    template <typename U>
    void Insert(U&& value)
    {
        const std::size_t index = GetInsertionIndex();

        data_[index] = std::forward<U>(value);

        UpdateAfterInsertion();
    }

    [[nodiscard]]
    std::size_t GetInsertionIndex() const noexcept
    {
        // 가득 찬 경우 (head + Capacity) % Capacity == head
        return (head_ + size_) % Capacity;
    }

    void UpdateAfterInsertion() noexcept
    {
        if (Full())
        {
            // 기존 head를 덮어썼으므로 다음 원소가 새로운 head가 된다.
            head_ = NextIndex(head_);
        }
        else
        {
            ++size_;
        }
    }

    [[nodiscard]]
    static constexpr std::size_t NextIndex(std::size_t index) noexcept
    {
        return (index + 1) % Capacity;
    }

private:
    std::array<T, Capacity> data_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
};